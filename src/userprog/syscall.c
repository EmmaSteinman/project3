#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "../devices/shutdown.h"
#include "../devices/input.h"
#include "../filesys/filesys.h"
#include "../filesys/file.h"
#include "threads/synch.h"
#include "process.h"
#include "threads/palloc.h"
#include "vm/page.h"

static void syscall_handler (struct intr_frame *);
void sys_exit (int status);
static tid_t sys_exec (const char* file);
static void sys_halt ();
static bool sys_create (const char* file, unsigned size);
int sys_wait (tid_t pid);
int sys_open (const char* file);
void sys_close (int fd);
int sys_filesize (int fd);
int sys_read (int fd, const void *buffer, unsigned size);
int sys_write (int fd, const void *buffer, unsigned size);
bool sys_remove (const char *file);
void sys_seek (int fd, unsigned position);
unsigned sys_tell (int fd);
void check_address (void* addr, struct intr_frame *f);
void release_locks (void);
void check_page (void* addr);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* SYS_EXIT */
void
sys_exit (int status)
{
  struct thread* cur = thread_current();
  lock_acquire(&cur->element->lock);
  cur->element->exit_status = status;
  lock_release(&cur->element->lock);

  thread_exit();
}


/* SYS_EXEC*/
static tid_t sys_exec (const char* file)
{
  tid_t ret_pid = -1;
  ret_pid = process_execute(file);
  return ret_pid;
}

/* SYS_HALT */
static void sys_halt()
{
  shutdown_power_off();
}

/* SYS_CREATE */
static bool sys_create (const char* file, unsigned size)
{
  // make sure the name of the file isn't empty
  if (strlen(file) == 0)
    return 0;
  // make sure the name of the file isn't too big
  // file names must be 14 characters or fewer
  if (strlen(file) > 14)
    return 0;
  lock_acquire(&file_lock);
  int ret = filesys_create(file, size);
  lock_release(&file_lock);
  return ret;
}

/* SYS_WAIT */
int sys_wait (tid_t pid)
{
  int ret = process_wait(pid);
  return ret;
}

/* function sys_open()
 * Open file and return file descriptor
 * return -1 if failed
 */
int sys_open (const char* file)
{
  int fd = -1;
  struct file *file_ptr = NULL;

  lock_acquire(&file_lock);
  file_ptr = filesys_open(file);
  lock_release(&file_lock);
  if (file_ptr != NULL) {
    /* add file to this thread's fd_list */
    struct thread *t = thread_current();
    struct fd_elem *fd_elem = malloc(sizeof(struct fd_elem));
    fd_elem->fd = t->next_fd;
    fd_elem->file = file_ptr;
    list_push_back(&t->fd_list, &fd_elem->elem);
    t->next_fd++;
    t->num_file++;
    fd = fd_elem->fd;
  }

  return fd;
}


void sys_close (int fd)
{
  lock_acquire(&file_lock);

  /* locate our file according to fd */
  struct list_elem *e;
  struct fd_elem *fd_close = NULL;
  struct file *file_ptr = NULL;
  struct thread *t = thread_current();

  for (e = list_begin (&t->fd_list); e != list_end (&t->fd_list);
       e = list_next (e))
    {
      fd_close = list_entry (e, struct fd_elem, elem);
      if(fd_close->fd == fd)
      {
        file_ptr = fd_close->file;
        if(file_ptr != NULL)
        {
          file_close(file_ptr);
          list_remove(e);
          free(fd_close);
          t->num_file--;
        }
        break;
      }
    }

  lock_release(&file_lock);
}

int
sys_filesize (int fd)
{
  struct thread* cur = thread_current();
  struct list_elem* e;
  struct file* file;
  for (e = list_begin (&cur->fd_list); e != list_end (&cur->fd_list);
       e = list_next (e))
  {
    struct fd_elem* entry = list_entry(e, struct fd_elem, elem);
    if (entry->fd == fd)
      file = entry->file;
  }
  // call file_length on the pointer to the file and return that value
  return file_length(file);
}


int sys_read (int fd, const void *buffer, unsigned size)
{
  /* read in file according to input type */
  int ret = -1;

  /* STDIN */
  if (fd == 0){
    for (uint32_t i = 0; i < size; i++)
      *(uint8_t*) (buffer + i) = input_getc();
    ret = size;
  }

  /* STDOUT */
  if (fd == 1){
    ret = -1;
  }

  /* OPEN FD */
  if (fd != 0 && fd != 1){
    struct list_elem *e;
    struct fd_elem *fd_read = NULL;
    struct file *file_ptr = NULL;
    struct thread *t = thread_current();

    for (e = list_begin (&t->fd_list); e != list_end (&t->fd_list);
         e = list_next (e)) {
      fd_read = list_entry (e, struct fd_elem, elem);
      if(fd_read->fd == fd){
        file_ptr = fd_read->file;
        if (file_ptr)
        {
          lock_acquire(&file_lock);
          ret = file_read(file_ptr, buffer, size);
          lock_release(&file_lock);
        }
        else
          ret = -1;
        break;

      }
    }
  }

  return ret;
}

int
sys_write (int fd, const void *buffer, unsigned size)
{
  /* read in file according to input type */
  int ret = -1;

  /* STDIN */
  if (fd == 0){
    ret = -1;
  }

  /* STDOUT */
  if (fd == 1){
    putbuf(buffer, size);
    ret = size;
  }

  /* OPEN FD */
  if (fd != 0 && fd != 1){
    struct list_elem *e;
    struct fd_elem *fd_write = NULL;
    struct file *file_ptr = NULL;
    struct thread *t = thread_current();

    for (e = list_begin (&t->fd_list); e != list_end (&t->fd_list);
         e = list_next (e)) {
      fd_write = list_entry (e, struct fd_elem, elem);
      if(fd_write->fd == fd){
        file_ptr = fd_write->file;
        if (file_ptr)
        {
          lock_acquire(&file_lock);
          ret = file_write(file_ptr, buffer, size);
          lock_release(&file_lock);
        }
        else
          ret = -1;

        break;
      }
    }
  }


  return ret;
}

bool sys_remove(const char *file){
  bool ret = 0;
  lock_acquire(&file_lock);
  ret = filesys_remove(file);
  lock_release(&file_lock);
  return ret;
}

void sys_seek (int fd, unsigned position)
{
  lock_acquire(&file_lock);

  struct list_elem *e;
  struct fd_elem *fd_seek = NULL;
  struct file *file_ptr = NULL;
  struct thread *t = thread_current();

  for (e = list_begin (&t->fd_list); e != list_end (&t->fd_list);
       e = list_next (e)) {
    fd_seek = list_entry (e, struct fd_elem, elem);
    if(fd_seek->fd == fd){
      file_ptr = fd_seek->file;
      if (file_ptr)
        file_seek(file_ptr, position);
      break;
    }
  }

  lock_release(&file_lock);
}

unsigned sys_tell (int fd)
{
  lock_acquire(&file_lock);

  struct list_elem *e;
  struct fd_elem *fd_tell = NULL;
  struct file *file_ptr = NULL;
  struct thread *t = thread_current();
  unsigned ret = 0;

  for (e = list_begin (&t->fd_list); e != list_end (&t->fd_list);
       e = list_next (e)) {
    fd_tell = list_entry (e, struct fd_elem, elem);
    if(fd_tell->fd == fd){
      file_ptr = fd_tell->file;
      if (file_ptr)
        ret = file_tell(file_ptr);
      break;
    }
  }

  lock_release(&file_lock);
  return ret;
}

static void
syscall_handler (struct intr_frame *f)
{
  // we need to check the address first
  // the system call number is on the user's stack in the user's virtual address space
  // terminates the process if the address is illegal
  check_address (f->esp, f);

  void* arg1;
  void* arg2;
  void* arg3;
  int i;

  // if we get to this point, the address is legal
  int sys_call_id = *(int*)f->esp;
  ASSERT (sys_call_id >= 0 && sys_call_id < 14);

  switch (sys_call_id){
    case SYS_HALT:
      sys_halt();
      break;

    case SYS_EXIT:
      arg1 = f->esp + 4;
      check_address (arg1, f);
      sys_exit(*(int*)arg1);
      break;

    case SYS_EXEC:
      arg1 = f->esp + 4;
      check_address (arg1, f);
      for (i = 0; i < 14; i++) // file names can only have 14 characters or fewer
        check_address(*(char**)arg1+i, f);
      f->eax = sys_exec(*(char**)arg1);
      break;

    case SYS_WAIT:
      arg1 = f->esp + 4;
      check_address (arg1, f);
      f->eax = sys_wait(*(tid_t*)arg1);
      break;

    case SYS_CREATE:
      arg1 = f->esp + 4;
      arg2 = arg1 + 4;
      check_address (arg1, f);
      for (i = 0; i < 14; i++) // file names can only have 14 characters or fewer
        check_address(*(char**)arg1+i, f);
      check_address (arg2, f);
      f->eax = sys_create(*(char**)arg1, *(unsigned*)arg2);
      break;

    case SYS_REMOVE:
      arg1 = f->esp + 4;
      check_address (arg1, f);
      sys_remove(*(int*)arg1);
      break;

    case SYS_OPEN:
      arg1 = f->esp + 4;
      check_address (arg1, f);
      for (i = 0; i < 14; i++) // file names can only have 14 characters or fewer
        check_address(*(char**)arg1+i, f);
      f->eax = sys_open(*(char**)arg1);
      break;

    case SYS_FILESIZE:
      arg1 = f->esp + 4;
      check_address(arg1, f);
      f->eax = sys_filesize(*(int*)arg1);
      break;

    case SYS_READ:
      arg1 = f->esp + 4;
      arg2 = f->esp + 8;
      arg3 = f->esp + 12;
      check_address (arg1, f);
      check_address (arg2, f);
      check_page (arg2); // make sure we aren't trying to write to an unwritable page
      check_address (arg3, f);
      for (i = 0; i < *(int*)arg3; i++)
        check_address (*(char**)arg2+i, f);
      f->eax = sys_read (*(int*)arg1, *(char**)arg2, *(int*)arg3);
      break;

    case SYS_WRITE:
      arg1 = f->esp + 4;
      arg2 = f->esp + 8;
      arg3 = f->esp + 12;
      check_address (arg1, f);
      check_address (arg2, f);
      check_address (arg3, f);
      for (i = 0; i < *(int*)arg3; i++)
        check_address(*(char**)arg2+i, f);
      f->eax = sys_write (*(int*)arg1, *(char**)arg2, *(int*)arg3);
      break;

    case SYS_SEEK:
      arg1 = f->esp + 4;
      arg2 = f->esp + 8;
      check_address(arg1, f);
      check_address(arg2, f);
      sys_seek(*(int*)arg1, *(unsigned*)arg2);
      break;

    case SYS_TELL:
      arg1 = f->esp + 4;
      check_address(arg1, f);
      f->eax = sys_tell(*(int*)arg1);
      break;

    case SYS_CLOSE:
      arg1 = f->esp + 4;
      check_address (arg1, f);
      sys_close(*(int**)arg1);
      break;
  }

}

/* New function to check whether an address passed in to a system call
   is valid, i.e. it is not null, it is not a kernel virtual address, and it is
   not unmapped. As of project 3, only kills the thread if the invalid address is
   not associated with an entry in the SPT or is not close to the stack. */
void
check_address (void* addr, struct intr_frame *f)
{
  int i;
  struct thread* cur = thread_current();

  for (i = 0; i < 4; i++)
  {
    // if the initial address is null or a kernel address, we definitely need to exit
    if (addr+i == NULL || is_kernel_vaddr(addr+i))
    {
      lock_acquire(&cur->element->lock);
      cur->element->exit_status = -1;
      lock_release(&cur->element->lock);
      thread_exit();
    }
    uint32_t* pd = thread_current()->pagedir;
    void* kernel_addr = pagedir_get_page(pd, addr+i);
    // kernel_addr is only NULL if addr points to unmapped virtual memory
    // if the user passed in an unmapped address, we want to exit
    if (kernel_addr == NULL)
    {
      struct hash_elem* e;
      struct page_table_elem p;
      p.page_no = pg_no (addr+i);
      lock_acquire(&cur->spt_lock);
      e = hash_find (&cur->s_page_table, &p.elem);
      lock_release(&cur->spt_lock);
      // if the page isn't in the supplemental page table, then we can't load it,
      // so kill the thread
      if (e == NULL)
      {
        // if the address should be invalid but it is close to the stack,
        // add a new page to the stack for it
        if ((int)f->esp - (int)addr <= 32 && (int)f->esp - (int)addr > -100000 && (int)f->esp - (int)addr != 0)
        {
          add_stack_page(f, addr+i);
          return;
        }
        lock_acquire(&cur->element->lock);
        cur->element->exit_status = -1;
        lock_release(&cur->element->lock);
        thread_exit();
      }
    }
  }
}

/* Checks that we are not trying to write to an unwritable page.
   If we are, kills the thread. */
void check_page (void* addr)
{
  struct thread* cur = thread_current();
  struct hash_elem* e;
  struct page_table_elem p;
  p.page_no = pg_no (*(char**)addr);
  lock_acquire(&cur->spt_lock);
  e = hash_find (&cur->s_page_table, &p.elem);
  struct page_table_elem* entry = hash_entry(e, struct page_table_elem, elem);
  lock_release(&cur->spt_lock);
  if (entry != NULL && entry->writable == false)
  {
    lock_acquire(&cur->element->lock);
    cur->element->exit_status = -1;
    lock_release(&cur->element->lock);
    thread_exit();
  }
}

/* Releases all locks held by a given thread. Called before a thread
   exits abnormally. */
void
release_locks (void) {
  struct list_elem* e;
  struct thread* cur = thread_current();
  for (e = list_begin (&cur->locks); e != list_end (&cur->locks);
        e = list_next (e))
  {
    struct lock* entry = list_entry(e, struct lock, elem);
    lock_release(entry);
  }
}
