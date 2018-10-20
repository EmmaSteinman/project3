
#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h" // NEW
#include "../devices/shutdown.h"
#include "../devices/input.h"
#include "../filesys/filesys.h"
#include "../filesys/file.h"
#include "threads/synch.h"
#include "process.h"

// TODO: write a function that gets arguments?

static void syscall_handler (struct intr_frame *);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* SYS_EXIT */
void
sys_exit(struct intr_frame *f) {

  void* arg1 = f->esp + 4;
  check_address (arg1, f);
  f->eax = *(int*)arg1;

  struct thread* cur = thread_current();
  lock_acquire(&cur->element->lock);
  cur->element->exit_status = *(int*)arg1;
  lock_release(&cur->element->lock);
  thread_exit();
}


/* SYS_EXEC*/
static tid_t sys_exec(struct intr_frame *f){
  char **file = f->esp + 4;
  tid_t ret_pid = -1;
  int i;
  for (i = 0; i < sizeof(file); i++)
    check_address(file+i, f);
  for (i = 0; i < sizeof(*file); i++)
    check_address((*file)+i, f);
  ret_pid = process_execute(*file);
  return ret_pid;
}

/* SYS_HALT */
static void sys_halt(){
  shutdown_power_off();
}

/* SYS_CREATE */
static bool sys_create(struct intr_frame *f)
{
  char **file = f->esp + 4;
  check_address (*file, f);
  void *size = file + 4;
  check_address (size, f);
  // make sure the name of the file isn't empty
  if (strlen(*file) == 0)
    return 0;
  // make sure the name of the file isn't too big
  // file names must be 14 characters or fewer
  if (strlen(*file) > 14)
    return 0;
  lock_acquire(&file_lock);
  int ret = filesys_create((char*)file, *(int*)size);
  lock_release(&file_lock);
  return ret;
}

/* SYS_WAIT */
int sys_wait (struct intr_frame *f)
{
  tid_t* pid = f->esp + 4;
  int ret = process_wait(*pid);
  return ret;
}

/* function sys_open()
 * Open file and return file descriptor
 * return -1 if failed
 */
int sys_open (const char* file){
  int fd = -1;
  struct file *file_ptr = NULL;

  lock_acquire(&file_lock);

  file_ptr = filesys_open(file);

  if (file_ptr) {
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
  lock_release(&file_lock);

  return fd;
}

int sys_close (int fd){
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
      if(fd_close->fd == fd){
        file_ptr = fd_close->file;
        if(file_ptr){
          file_close(file_ptr);
          list_remove(e);
          t->num_file--;
          free(fd_close);
        }
      }
    }

  lock_release(&file_lock);

  return 0;
}



int sys_read(int fd, const void *buffer, unsigned size){
  /* read in file according to input type */
  lock_acquire(&file_lock);

  int ret = -1;

  /* STDIN */
  if (fd == STDIN_FILENO){
    for (uint32_t i = 0; i < size; i++)
      *(uint8_t*) (buffer + i) = input_getc();
    ret = size;
  }

  /* STDOUT */
  if (fd == STDOUT_FILENO){
    ret = -1;
  }

  /* OPEN FD */
  if (fd != STDIN_FILENO && fd != STDOUT_FILENO){
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
          ret = file_read(file_ptr, buffer, size);
        else
          ret = -1;
      }
    }
  }

  lock_release(&file_lock);

  return ret;
}

int
sys_write (int fd, const void *buffer, unsigned size)
{
  /* read in file according to input type */
  lock_acquire(&file_lock);

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
  if (fd != STDIN_FILENO && fd != STDOUT_FILENO){
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
          ret = file_write(file_ptr, buffer, size);
        else
          ret = -1;
      }
    }
  }

  lock_release(&file_lock);
}

static void
syscall_handler (struct intr_frame *f)
{
  // there are at most 3 arguments to each system call
  // and each takes up 4 bytes on the stack

  // we need to check the address first
  // the system call number is on the user's stack in the user's virtual address space
  // so check the address first
  // terminates the process if the address is illegal
  check_address (f->esp, f);

  void* arg1;
  void* arg2;
  void* arg3;

  // if we get to this point, the address is legal
  int sys_call_id = *(int*)f->esp;
  // TODO: assert to ensure that this is actually a valid syscall number
  ASSERT (sys_call_id >= 0 && sys_call_id < 14);

  switch (sys_call_id){
    case SYS_HALT:
      sys_halt();
      break;

    case SYS_EXIT:
      sys_exit(f);
      break;

    case SYS_EXEC:
      f->eax = sys_exec(f);
      break;

    case SYS_WAIT:
      //f->eax = process_wait((tid_t)(f->esp + 4));
      f->eax = sys_wait(f);
      break;

    case SYS_CREATE:
      f->eax = sys_create(f);
      break;

    case SYS_REMOVE:
      break;

    case SYS_OPEN:
      arg1 = f->esp + 4;
      check_address (arg1, f);
      f->eax = sys_open(*(char**)arg1);
      break;

    case SYS_FILESIZE:
      break;

    case SYS_READ:
      arg1 = f->esp + 4;
      arg2 = f->esp + 8;
      arg3 = f->esp + 12;
      check_address (arg1, f);
      check_address (arg2, f);
      check_address (arg3, f);
      f->eax = sys_read (*(int*)arg1, *(char**)arg2, *(int*)arg3);
      break;

    case SYS_WRITE:
      arg1 = f->esp + 4;
      arg2 = f->esp + 8;
      arg3 = f->esp + 12;
      check_address (arg1, f);
      check_address (arg2, f);
      check_address (arg3, f);
      f->eax = sys_write (*(int*)arg1, *(char**)arg2, *(int*)arg3);
      break;

    case SYS_SEEK:
      break;

    case SYS_TELL:
      break;

    case SYS_CLOSE:
      arg1 = f->esp + 4;
      check_address (arg1, f);
      f->eax = sys_close(*(int*)arg1);
      break;
  }

}

/* New function to check whether an address passed in to a system call
   is valid, i.e. it is not null, it is not a kernel virtual address, and it is
   not unmapped. */
void
check_address (void* addr, struct intr_frame *f)
{
  // TODO: do we need to do something extra to deallocate memory?
  //       maybe just ensure synchronization so that the thread is
  //       deallocated before continuing? idk how that would work
  int i;
  for (i = 0; i < 4; i++)
  {
    // if the initial address is null or a kernel address, we definitely need to exit
    if (addr+i == NULL || is_kernel_vaddr(addr+i))
    {
      //release_locks ();
      struct thread* cur = thread_current();
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
      //release_locks ();
      struct thread* cur = thread_current();
      lock_acquire(&cur->element->lock);
      cur->element->exit_status = -1;
      lock_release(&cur->element->lock);
      thread_exit();
    }
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
