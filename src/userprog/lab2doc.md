
# Project 2 Design Document

> Denison cs372  
> Fall 2018

## GROUP

> Fill in the name and email addresses of your group members.

- Hayley LeBlanc <leblan_h1@denison.edu>
- Desmond Liang <liang_d1@denison.edu>
- Bryan Tran <tran_b1@denison.edu>

## PRELIMINARIES

> If you have any preliminary comments on your submission, notes for the
> Instructor, or extra credit, please give them here.

> Please cite any offline or online sources you consulted while
> preparing your submission, other than the Pintos documentation, course
> text, lecture notes, and course staff.

## PROJECT PARTS

> In the documentation that you provide in the following, you need to provide clear, organized, and coherent documentation, not just short, incomplete, or vague "answers to the questions".  The questions are provided to help you structure your thinking around the information you need to convey.

### A. ARGUMENT PASSING  

#### DATA STRUCTURES

> A1: Copy here the declaration of each new or changed `struct` or
> `struct` member, `global` or `static` variable, `typedef`, or
> enumeration.  Document the purpose of each in 25 words or less.

#### ALGORITHMS

> A2: Briefly describe how you implemented argument parsing.  How do
> you arrange for the elements of argv[] to be in the right order?
> How do you avoid overflowing the stack page?

We do all of the argument parsing and passing in the `setup_stack()` function. We initially tried implementing it in `process_start()`, but had trouble getting it to work there. Placing this code in `setup_stack()` makes sense because putting the arguments to a process on the stack is indeed part of setting up the stack for the new process. However, it does require us to also parse the name of the file to load and run earlier on, which is likely not the most efficient way to do it.

In `setup_stack()`, if allocation of the new thread's page is successful, we parse the arguments from the command line using `strtok_r()` and place them in an array of character arrays called `args`. There is an arbitrary limit of 32 arguments per process. Then, we push information onto the stack as specified in the project description, starting at the top of the stack (higher addresses) and going down.

First, we write each string in the in `args` to the stack. Each time we write one string, we save its address on the stack in another array, `argv`. We then write some zeroes to the stack to align following data on the word boundary, and write the contents of `argv` to the stack. Both times we write an array to the stack, we start writing with its last element and end with its first. This ensures that the pointers to the arguments are in the right order. At the end of writing the contents of `argv`, we also save the address of the first argument (the last one we wrote) and write that address to the bottom of the stack as well. This ensures that the program can find the pointers to the actual arguments. At the end, we write `argc` (the number of actual arguments) and a null pointer to the end of the stack.

We don't have any specific checks to make sure that we don't overflow the stack page, but the limit on the number of arguments should prevent it in most cases.

#### RATIONALE/JUSTIFICATION

> A3: Why does Pintos implement strtok_r() but not strtok()?

According to the `strtok()` man page, `strtok_r()` is thread-safe but `strtok()` is not. We may have multiple threads trying to tokenize strings at the same time, so we need to use a version that is thread-safe.

> A4: In Pintos, the kernel separates commands into a executable name
> and arguments.  In Unix-like systems, the shell does this
> separation.  Identify at least two advantages of the Unix approach.

At least for us, it was difficult to find a location in the kernel code where we could (at last somewhat) efficiently implement argument parsing in a way that actually worked. If we were able to parse the arguments in the shell, this would not be an issue, because the arguments could be separated and saved into an array immediately after they are entered by the user. Thus, it would be an advantage on the designer's end simply because it would be easier to implement. Also, parsing arguments in the shell might allow users to have more control over how they want their arguments to be interpreted - perhaps they could set specific settings with the shell that allow them to use shortcuts or type different things but get the same result. This would not be possible if the kernel parsed the arguments because those settings would have to be programmed beforehand.

### SYSTEM CALLS

#### DATA STRUCTURES

> B1: Copy here the declaration of each new or changed `struct` or
> `struct` member, `global` or `static` variable, `typedef`, or
> enumeration.  Document the purpose of each in 25 words or less.

New fields in `thread` struct:

- `struct semaphore process_sema;`: a semaphore used by `process_wait()` to block a parent thread until its child is finished executing.
- `struct thread_elem* element;`: the node associated with this thread in the `thread_list` list.
- `struct semaphore exec_sema;`: a semaphore used to synchronize thread creation with `process_execute()`.

Functions:
- `struct thread* get_thread_all (tid_t tid)` in thread.c. Given a thread ID, searches the list of all threads and returns a pointer to the thread associated with that TID, if it exists.
- `void sys_exit(struct intr_frame *f)` in syscall.c. Implements the `exit` system call.
- `void sys_write(struct intr_frame *f)` in syscall.c. Implements the `write` system call.
- `static tid_t sys_exec(struct intr_frame *f)` in syscall.c. Implements the `exec` system call.
- `static void sys_halt()` in syscall.c. Implements the `halt` system call.
- `static bool sys_create(struct intr_frame *f)` in syscall.c. Implements the `create` system call.
- `int sys_wait (struct intr_frame *f)` in syscall.c. Implements the `wait` system call.
- `void check_address (void* addr, struct intr_frame *f)` in syscall.c. Checks a given address to ensure that the current thread has legal access to it, and kills the thread otherwise.
- `int write (int fd, const void *buffer, unsigned length)` in syscall.c.

New declarations in thread.h:

- `struct list thread_list;`: a list where each node is associated with a thread that is either currently alive (blocked, running, or ready) or a thread that has recently died. Each `list_elem` in this list is part of a `thread_elem` struct. We remove elements from the list once the associated thread has been waited on once by its parent.

```
struct thread_elem
  {
    struct list_elem elem;
    tid_t* tid;
    int exit_status;
    struct thread* parent;
  };
```
The `thread_elem` struct contains the information associated with each node in the `thread_list` list. The `list_elem` field is inserted into `thread_list`. Each thread points to the `thread_elem` in the list that contains its information, although some `thread_elem`s are associated with dead threads that have been deallocated.

- `struct lock file_lock;`: A lock used to prevent race conditions when we access the file system.


> B2: Describe how file descriptors are associated with open files.
> Are file descriptors unique within the entire OS or just within a
> single process?

#### ALGORITHMS

> B3: Describe your code for reading and writing user data from the
> kernel.

> B4: Suppose a system call causes a full page (4,096 bytes) of data
> to be copied from user space into the kernel.  What is the least
> and the greatest possible number of inspections of the page table
> (e.g. calls to pagedir_get_page()) that might result?  What about
> for a system call that only copies 2 bytes of data?  Is there room
> for improvement in these numbers, and how much?

> B5: Briefly describe your implementation of the "wait" system call
> and how it interacts with process termination.

**do this once multi-recurse and multi-oom tests work**

> B6: Any access to user program memory at a user-specified address
> can fail due to a bad pointer value.  Such accesses must cause the
> process to be terminated.  System calls are fraught with such
> accesses, e.g. a "write" system call requires reading the system
> call number from the user stack, then each of the call's three
> arguments, then an arbitrary amount of user memory, and any of
> these can fail at any point.  This poses a design and
> error-handling problem: how do you best avoid obscuring the primary
> function of code in a morass of error-handling?  Furthermore, when
> an error is detected, how do you ensure that all temporarily
> allocated resources (locks, buffers, etc.) are freed?  In a few
> paragraphs, describe the strategy or strategies you adopted for
> managing these issues.  Give an example.

#### SYNCHRONIZATION

> B7: The "exec" system call returns -1 if loading the new executable
> fails, so it cannot return before the new executable has completed
> loading.  How does your code ensure this?  How is the load
> success/failure status passed back to the thread that calls "exec"?

Our `exec` system call calls `process_execute()` in process.c and simply returns the value returned by that function, so all of the synchronization work happens in `process_execute()`. The `process_execute()` function does a bit of work, then calls `thread_create()` to start a new thread in the function `start_process()`. `start_process()` calls `load()` to load the executable for the child process to run in `start_process()`. The parent process needs to wait until the child has attempted to load the executable (successfully or not) before it can continue, because the parent process needs to know whether the load failed before it can return.

We enforce this using a semaphore called `exec_sema` that is part of the parent thread's `thread` struct. Immediately after calling and returning from `thread_create()` (i.e., as soon as possible), we call `sema_down()` on the parent's `exec_sema` to force it to wait. The child process then calls `sema_up()` on this same semaphore once its call to `load()` has finished. If the call wasn't successful, we set the child process's exit code to -1, then call `sema_up()`; otherwise, we just call `sema_up()` as soon as the load is done. Calling `sema_up()` after the child has attempted to load blocks the parent until we know the outcome of `load()`.

Once the parent starts executing again, it checks the child's exit status; if it's -1, we know that the load failed, so we return -1 from `process_execute()`. Otherwise (unless some other unrelated issue has occurred), we return the thread ID of the new child thread.

> B8: Consider parent process P with child process C.  How do you
> ensure proper synchronization and avoid race conditions when P
> calls wait(C) before C exits?  After C exits?  How do you ensure
> that all resources are freed in each case?  How about when P
> terminates without waiting, before C exits?  After C exits?  Are
> there any special cases?

#### RATIONALE

> B9: Why did you choose to implement access to user memory from the
> kernel in the way that you did?

> B10: What advantages or disadvantages can you see to your design
> for file descriptors?

> B11: The default tid_t to pid_t mapping is the identity mapping.
> If you changed it, what advantages are there to your approach?
