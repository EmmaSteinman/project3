
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
// TODO: ALL SECTIONS THAT ACCESS FILES SHOULD BE TREATED AS CRITICAL SECTIONS

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

  cur->element->exit_status = *(int*)arg1;
  thread_exit();
}

/* SYS_WRITE */
// TODO: combine this function with the write() function
void sys_write(struct intr_frame *f){
  /* take the 3 arguments to the system call off the stack */
  void* sys_write_arg1 = f->esp + 4;
  check_address (sys_write_arg1, f);
  void* sys_write_arg2 = sys_write_arg1 + 4;
  check_address (sys_write_arg2, f);
  void* sys_write_arg3 = sys_write_arg2 + 4;
  check_address (sys_write_arg3, f);
  /* pass the arguments (with correct casts) to write */
  int ret = write (*(int*)sys_write_arg1, *(char**)sys_write_arg2,
    *(int*)sys_write_arg3);
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

  // if we get to this point, the address is legal
  int sys_call_id = *(int*)f->esp;
  // TODO: assert to ensure that this is actually a valid syscall number

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
      break;

    case SYS_FILESIZE:
      break;

    case SYS_READ:
      break;

    case SYS_WRITE:
      sys_write(f);
      break;

    case SYS_SEEK:
      break;

    case SYS_TELL:
      break;

    case SYS_CLOSE:
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
      release_locks ();
      struct thread* cur = thread_current();
      cur->element->exit_status = -1;
      thread_exit();
    }

    uint32_t* pd = thread_current()->pagedir;
    void* kernel_addr = pagedir_get_page(pd, addr+i);
    // kernel_addr is only NULL if addr points to unmapped virtual memory
    // if the user passed in an unmapped address, we want to exit
    if (kernel_addr == NULL)
    {
      release_locks ();
      struct thread* cur = thread_current();
      cur->element->exit_status = -1;
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

int
write (int fd, const void *buffer, unsigned length)
{
  // TODO: implement other kinds of writing
  if (fd == 1)
    putbuf(buffer, length);
  return 0; // should return the number of bytes actually written
}
