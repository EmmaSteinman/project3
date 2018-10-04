#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h" // NEW

static void syscall_handler (struct intr_frame *);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f)
{
  // there are at most 3 arguments to each system call
  // and each takes up 4 bytes on the stack

  // we need to check the address first
  // the system call number is on the user's stack in the user's virtual address space
  // so check the address first
  // terminate the process if the address is illegal
  check_address (f->esp);

  // if we get to this point, the address is legal
  int sys_call_id = *(int*)f->esp;
  // TODO: assert to ensure that this is actually a valid syscall number

  if (sys_call_id == SYS_HALT)
    printf("sys halt\n");
  else if (sys_call_id == SYS_EXIT)
    {
      // TODO: there is definitely more to do here
      printf("sys exit\n");
      // need to get the argument to pass to the call off of the stack
      void* arg1 = f->esp + 4;
      check_address (arg1);
      //exit (*(int*)arg1);
      f->eax = *(int*)arg1;
      thread_exit();
    }
  else if (sys_call_id == SYS_EXEC)
    printf("sys exec\n");
  else if (sys_call_id == SYS_WAIT)
    printf("sys wait\n");
  else if (sys_call_id == SYS_CREATE)
    printf("sys create\n");
  else if (sys_call_id == SYS_REMOVE)
    printf("sys remove\n");
  else if (sys_call_id == SYS_OPEN)
    printf("sys open\n");
  else if (sys_call_id == SYS_FILESIZE)
    printf("sys filesize\n");
  else if (sys_call_id == SYS_READ)
    printf("sys read\n");
  else if (sys_call_id == SYS_WRITE)
    {
      printf("sys write\n");
      // take the 3 arguments to the system call off the stack
      void* arg1 = f->esp + 4;
      check_address (arg1);
      void* arg2 = arg1 + 4;
      check_address (arg2);
      void* arg3 = arg2 + 4;
      check_address (arg3);
      // pass the arguments (with correct casts) to write
      int ret = write (*(int*)arg1, *(char**)arg2, *(int*)arg3);
    }
  else if (sys_call_id == SYS_SEEK)
    printf("sys seek\n");
  else if (sys_call_id == SYS_TELL)
    printf("sys tell\n");
  else if (sys_call_id == SYS_CLOSE)
    printf("sys close\n");

  printf ("system call!\n");
  //thread_exit ();
}

/* New function to check whether an address passed in to a system call
   is valid, i.e. it is not null, it is not a kernel virtual address, and it is
   not unmapped. */
void
check_address (void* addr)
{
  // do we need to do something special to account for the fact that each argument on the
  // stack to a system call gets 4 bytes?
  //TODO: releasing locks?

  // if the initial address is null or a kernel address, we definitely need to exit
  if (addr == NULL || is_kernel_vaddr(addr))
  {
    printf("EXITING\n");
    //process_exit();
    thread_exit();
  }
  uint32_t* pd = thread_current()->pagedir;
  void* kernel_addr = pagedir_get_page(pd, addr);
  // kernel_addr is only NULL if addr points to unmapped virtual memory
  // if the user passed in an unmapped address, we want to exit
  if (kernel_addr == NULL)
  {
    printf("EXITING\n");
    //process_exit(); // thread_exit already calls process exit
    thread_exit();
  }
}

/* Exit system call. Doesn't do anything yet. */
// void
// exit (int status) {
//   printf("%i\n", status);
//   // TODO: actually implement this
//   thread_exit();
// }

int
write (int fd, const void *buffer, unsigned length)
{
  // TODO: implement other kinds of writing
  if (fd == 1)
    putbuf(buffer, length);
  return 0; // should return the number of bytes actually written
}
