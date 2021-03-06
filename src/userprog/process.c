


#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hash.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/frame.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name)
{
  char *fn_copy;
  tid_t tid;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0); // don't replace with allocate_page because this pulls from kernel pool
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  // extract ONLY the file name so we can name the process
  // since strtok_r changes the arg string, we need to make a copy
  // of file_name before doing this
  char* copy2, *save_ptr;
  copy2 = palloc_get_page (0); // don't replace with allocate_page because this pulls from kernel pool
  if (copy2 == NULL)
    return TID_ERROR;
  strlcpy (copy2, file_name, PGSIZE);
  char* fn = strtok_r(copy2, " ", &save_ptr);

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (fn, PRI_DEFAULT, start_process, fn_copy);
  palloc_free_page(copy2);
  if (tid == TID_ERROR)
  {
    palloc_free_page (fn_copy);
    return tid;
  }
  sema_down(&thread_current()->exec_sema);
  struct thread* child_thread = get_thread_all(tid);

  // if the child is null and its exit status is -1, return -1
  if (child_thread == NULL)
    {
      struct list_elem* e;
      for (e = list_begin (&thread_list); e != list_end (&thread_list);
            e = list_next (e))
      {
        struct thread_elem* entry = list_entry(e, struct thread_elem, elem);
        lock_acquire(&entry->lock);
        if (entry->exit_status == -1 && entry->tid == tid)
          {
            lock_release(&entry->lock);
            return -1;
          }
        lock_release(&entry->lock);
      }
    }

  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;
  struct thread* cur = thread_current();

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  lock_acquire(&file_lock);
  success = load (file_name, &if_.eip, &if_.esp);
  lock_release(&file_lock);

  palloc_free_page (file_name);
  if (!success)
  {
    lock_acquire(&cur->element->lock);
    cur->element->exit_status = -1;
    lock_release(&cur->element->lock);
    sema_up(&cur->element->parent->exec_sema);
    thread_exit ();
  }
  sema_up(&cur->element->parent->exec_sema);

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */

  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */

int
process_wait (tid_t child_tid)
{
  struct thread* cur = thread_current();
  struct thread* child_thread = get_thread_all (child_tid);
  struct thread_elem* elem;

  if (child_thread == NULL)
  {
    struct list_elem* e;
    for (e = list_begin (&thread_list); e != list_end (&thread_list);
          e = list_next (e))
    {
      struct thread_elem* entry = list_entry(e, struct thread_elem, elem);
      lock_acquire(&entry->lock);
      if (entry->tid == child_tid && entry->parent == cur)
      {
        int status = entry->exit_status;
        list_remove(&entry->elem);
        lock_release(&entry->lock);
        free(entry);
        return status;
      }
      // if a thread_elem's thread has died and its parent is null, then
      // there's no point in keeping this thread_elem around,
      // since nothing can wait on it and we'll never have to return
      // its exit status
      else if (entry->parent == NULL && entry->thread == NULL)
      {
        list_remove(&entry->elem);
        lock_release(&entry->lock);
        free(entry);
      }
      else
      {
        lock_release(&entry->lock);
      }
    }
    return -1;
  } else {
    elem = child_thread->element;
    lock_acquire(&elem->lock);
    if (elem->parent != cur)
      {
        lock_release(&elem->lock);
        return -1;
      }
    lock_release(&elem->lock);
  }

  lock_acquire(&elem->lock);
  list_remove(&elem->elem); // remove the child thread's element from the list since we have now waited on it
  lock_release(&elem->lock);
  sema_down(&child_thread->process_sema);

  lock_acquire(&elem->lock);
  int status = elem->exit_status;
  lock_release(&elem->lock);

  free(elem);

  return status;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL)
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp, char *file_name);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable, char** name);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp)
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL)
    goto done;
  process_activate ();

  // make a copy of file_name and extract only the name of the executable
  char* copy, *save_ptr;
  copy = palloc_get_page (0); // don't replace with allocate_page because this pulls from kernel pool
  if (copy == NULL)
    return TID_ERROR;
  strlcpy (copy, file_name, PGSIZE);
  char* fn = strtok_r(copy, " ", &save_ptr);

  // save the file name for use later
  char** name = malloc(sizeof(fn));
  strlcpy (name, fn, strlen(fn)+1);

  /* Open executable file. */
  file = filesys_open (fn);
  palloc_free_page(copy); // free page to prevent memory leak
  if (file == NULL)
    {
      printf ("load: %s: open failed\n", file_name);
      goto done;
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024)
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done;
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++)
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type)
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file))
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable, name))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp, file_name))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;
  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  file_close (file);
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file)
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
    return false;

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file))
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz)
    return false;

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;

  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable, char** name)
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  int pos = 0;
  while (read_bytes > 0 || zero_bytes > 0)
    {

      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      // put this page into the page table
      struct thread* t = thread_current();

      struct page_table_elem* entry = malloc(sizeof(struct page_table_elem));
      entry->t = t;
      entry->addr = upage;
      entry->ofs = ofs;
      entry->page_read_bytes = page_read_bytes;
      entry->page_zero_bytes = page_zero_bytes;
      entry->page_no = pg_no(upage);
      entry->name = name;
      entry->writable = writable;
      entry->pos = pos;
      entry->swapped = false;

      lock_acquire (&t->spt_lock);
      struct hash_elem* h = hash_insert (&t->s_page_table, &entry->elem);
      lock_release (&t->spt_lock);

      /* Advance. */
      read_bytes -= page_read_bytes;
      pos += page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }

  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. NEW: also puts the argument of the user
   program on the stack. */
static bool
setup_stack (void **esp, char *file_name)
{
  uint8_t *kpage;
  bool success = false;

  //kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  kpage = allocate_page (PAL_ZERO);
  if (kpage != NULL)
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
      {
        *esp = PHYS_BASE;

        // put arguments on the stack
        char* args[32];
        char* argv[32];
        int argc = 0;
        char* token, *save_ptr;

        // get the tokens
        for (token = strtok_r (file_name, " ", &save_ptr); token != NULL;
            token = strtok_r (NULL, " ", &save_ptr))
            {
              args[argc] = token;
              argc++;
            }
        *esp -= 1;

        // write strings to the stack
        // we need to be dereferencing esp
        int i;
        for (i = argc-1; i >=0; i--)
        {
          *esp -= strlen(args[i])+1; // subtract 1 more than strlen to include null terminating character
          // memcpy is from string.c
          // necessary because we can't directly copy something into the value
          // at esp since esp is a void pointer pointer
          memcpy(*esp, args[i], strlen(args[i]));
          argv[i] = *esp;
        }

        // word align
        if ((int)*esp % 4 != 0)
        {
          *esp -= 1;
          uint8_t zero = 0x0;
          memcpy(*esp, &zero, 1);
        }

        // null pointer for end of argv
        *esp -= sizeof(char*);
        // TODO: do we need to set the values here to 0?

        // argv pointers
        char** argvptr;
        for (i = argc-1; i >= 0; i--)
        {
          *esp -= sizeof(argv[i]);
          memcpy(*esp, &argv[i], sizeof(argv[i]));
          if (i == 0)
            argvptr = *esp;
        }

        // pointer to the beginning of argv
        *esp -= sizeof(argvptr);
        memcpy(*esp, &argvptr, sizeof(argvptr));

        // argc
        *esp -= sizeof(argc);
        memcpy(*esp, &argc, sizeof(argc));

        // fake return address
        int z = 0;
        *esp -= sizeof(z);
        memcpy(*esp, &z, sizeof(z));

        // checks for stack overflow
        if (!is_thread(thread_current()))
          success = false;
      }
      else
        palloc_free_page (kpage);
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
