
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

The `rox_simple` test passes.

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

We do all of the argument parsing and passing in the `setup_stack()` function. We initially tried implementing it in `process_start()`, but had trouble passing the parsed arguments through to the new process. Placing this code in `setup_stack()` makes sense because putting the arguments to a process on the stack is indeed part of setting up the stack for the new process. However, it does require us to also parse the name of the file to load and run earlier on, which is likely not the most efficient way to do it.

In `setup_stack()`, if allocation of the new thread's page is successful, we parse the arguments from the command line using `strtok_r()` and place them in an array of character arrays called `args`. There is an arbitrary limit of 32 arguments per process. Then, we push information onto the stack as specified in the project description, starting at the top of the stack (higher addresses) and going down.

First, we write each string in the in `args` to the stack. Each time we write one string, we save its address on the stack in another array, `argv`. We then write some zeroes to the stack to align following data on the word boundary, and write the contents of `argv` to the stack. Both times we write an array to the stack, we start writing with its last element and end with its first. This ensures that the pointers to the arguments are in the right order. At the end of writing the contents of `argv`, we also save the address of the first argument (the last one we wrote) and write that address to the bottom of the stack. This ensures that the program can find the pointers to the actual arguments. At the end, we write `argc` (the number of actual arguments) and a null return address pointer to the end of the stack.

To avoid overflowing the stack page, we call `is_thread()` at the end of the section of code that pushes arguments onto the stack. This takes advantage of built-in thread functions and fields to check whether the thread's page has been overflowed. Each thread is allocated one 4KB page in memory. This page is organized as follows:

    4 kB +---------------------------------+
         |          kernel stack           |
         |                |                |
         |                |                |
         |                V                |
         |         grows downward          |
         |                                 |
         |                                 |
         |                                 |
         |                                 |
         |                                 |
         |                                 |
         |                                 |
         |                                 |
         +---------------------------------+
         |              magic              |
         |                :                |
         |                :                |
         |               name              |
         |              status             |
    0 kB +---------------------------------+

The thread's stack is located at the top of the page, and its `thread` struct is located at the bottom. The thread's `magic` field is located near the top of the `thread` struct, and its only purpose is to be used to check for stack overflow. As the stack grows downward, if it grows too large, it will overwrite the thread's `magic` value. So, in order to check for stack overflow, we can just check a thread's `magic` value. If it still matches the global `THREAD_MAGIC` constant, then the stack has not overflowed; if it doesn't, then the stack has overflowed and overwritten data at the top of the `thread` struct, and the program should exit. The `is_thread()` function returns `false` if the current thread's `magic` field doesn't match `THREAD_MAGIC`, so we simply call this function and change the `success` variable in `setup_stack()` to `false` if it returns `false`. This causes `setup_stack()` to return `false`, which ultimately causes the call to `load()` in `start_process()` to fail.

#### RATIONALE/JUSTIFICATION

> A3: Why does Pintos implement strtok_r() but not strtok()?

According to the `strtok()` man page, `strtok_r()` is thread-safe but `strtok()` is not. We may have multiple threads trying to tokenize strings concurrently, so we need to use a version that is thread-safe.

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
- `struct list locks;`: a list of locks currently held by this thread.

New fields in `lock` struct:
- `struct list_elem elem;`: a list element that allows this lock to be added to a thread's `locks` list.

Functions:
- `struct thread* get_thread_all (tid_t tid)` in thread.c. Given a thread ID, searches the list of all threads and returns a pointer to the thread associated with that TID, if it exists.
- `void sys_exit (int status)` in syscall.c. Implements the `exit` system call.
- `static tid_t sys_exec (const char* file)` in syscall.c. Implements the `exec` system call.
- `static void sys_halt ()` in syscall.c. Implements the `halt` system call.
- `static bool sys_create (const char* file, unsigned size)` in syscall.c. Implements the `create` system call.
- `int sys_wait (tid_t pid)` in syscall.c. Implements the `wait` system call.
- `int sys_open (const char* file)` in syscall.c. Implements the `open` system call.
- `void sys_close (int fd)` in syscall.c. Implements the `close` system call.
- `int sys_filesize (int fd)` in syscall.c. Implements the `filesize` system call.
- `int sys_read (int fd, const void *buffer, unsigned size)` in syscall.c. Implements the `read` system call.
- `int sys_write (int fd, const void *buffer, unsigned size)` in syscall.c. Implements the `write` system call.
- `bool sys_remove (const char *file)` in syscall.c. Implements the `remove` system call.
- `void sys_seek (int fd, unsigned position)` in syscall.c. Implements the `seek` system call.
- `unsigned sys_tell (int fd)` in syscall.c. Implements the `tell` system call.
- `void check_address (void* addr, struct intr_frame *f)` in syscall.c. Checks a given address to ensure that the current thread has legal access to it, and kills the thread otherwise.
- `release_locks (void)` in syscall.c. Releases all of the locks held by a current thread. Called in `thread_exit()` to ensure that a thread's locks are all released before it dies.

New declarations in thread.h:

- `struct list thread_list;`: a list where each node is associated with a thread that is either currently alive (blocked, running, or ready) or a thread that has recently died. Each `list_elem` in this list is part of a `thread_elem` struct. We remove elements from the list once the associated thread has been waited on once by its parent.

```
struct thread_elem
  {
    struct list_elem elem;
    tid_t* tid;
    int exit_status;
    struct thread* parent;
    struct thread* thread;
  };
```
The `thread_elem` struct contains the information associated with each node in the `thread_list` list. The `list_elem` field is inserted into `thread_list`. Each thread points to the `thread_elem` in the list that contains its information, although some `thread_elem`s are associated with dead threads that have been deallocated.

- `struct lock file_lock;`: A lock used to prevent race conditions when we access the file system.

```
struct fd_elem
  {
    int fd;
    struct list_elem elem;
    struct file *file;
  };
```
The struct `fd_elem` is a file descriptor node that contains the fd number and a pointer to the file's location. The `list_elem` field is inserted into the doubly linked list `fd_list` field in the `thread` struct.

> B2: Describe how file descriptors are associated with open files.
> Are file descriptors unique within the entire OS or just within a
> single process?

File descriptors are associated with open files using `struct fd_elem` which has a integer fd number and a pointer to the file location. FD numbers are assigned to a file once its opened by a thread, starting from 2. fd number 0 and 1 are reserved for STDIN and STDOUT. In our implementation, file descriptors are not unique within the entire OS but within a single thread. Since each thread keep a list of open files, each thread can only access files opened by itself. This way we can maximize security so no malicious thread can access other thread's open files.

#### ALGORITHMS

> B3: Describe your code for reading and writing user data from the
> kernel.

To read arguments to system calls off of the user stack, we simply check the address of the provided interrupt frame's stack pointer using our `check_address()` function (described below in the answer to B6). If that check does not kill the thread, then we dereference the stack pointer to get the value of the argument. If the argument is itself a pointer to an array, we also check the pointers in that array before continuing. We do this to determine which system call has been called in `syscall_handler`. We then use that information to enter a case in a switch statement, where we pop several values off the stack (how many depends on how many arguments the specific system call takes) and use `check_address()` to verify their validity.

To pass the return value of a system call back to the user, we set the interrupt frame's `eax` register to the return value of the function that implements the system call we want. This is where the user program expects the return value to be.

For a user program to be able to read and write to files, it needs to have previously opened a valid, existing file. Our `open` system call takes a string and uses `filesys_open()` to attempt to open this file; if that call fails, which happens if a file with specified file name doesn't exist or if the user program otherwise doesn't have access to it for some reason, then the file is NOT added to the thread's list of open threads, and `open` returns -1. This prevents a user program from reading or writing to a file that it has not opened or cannot open.

If a user program calls the `read` system call, we need to put data from a file into a user-program-accessible buffer. The user provides a file descriptor `fd`, a `buffer` into which to write the data, and a `size` specifying the number of bytes to be read. An `fd` of 0 corresponds to STDIN, so if we get that as the file descriptor, we simply use the provided function `input_getc()` to read `size` characters from the keyboard into `buffer`. An `fd` of 1 corresponds to STDOUT, which can't be used in a `read` system call, so we return -1 if we are passed this file descriptor. Otherwise, the user is trying to access a file in the file system, so we search the current thread's list of open files to find one with file descriptor `fd`. If that file descriptor corresponds to a file that this thread currently has open, we call `file_read()`, which takes care of reading the correct number of bytes into `buffer`. If some part of this process fails - for example, if there is no file corresponding to `fd` in the thread's list of open files - we return -1. Otherwise, we return the number of bytes that were successfully read by `file_read()`.

If the user program calls the `write` system call, then we need to put data from a user-provided buffer into a file. The user provides the same arguments as for the `read` system call, except this time `buffer` already contains data that needs to be written. We return -1 if the user specifies an `fd` of 0, since STDIN is not a valid thing to write to. If `fd` is 1, then we use `putbuf()` to write the specified number of bytes to the console. For all other `fd` values, the process is almost exactly the same as with the `read` system call, except that once we find the open file associated with `fd` for the current thread, we use `file_write()` to write the buffer to that file. Again, we return the number of bytes written to the file (which may be fewer than `size` if we reach the end of the file), or -1 if some part of the process failed.

> B4: Suppose a system call causes a full page (4,096 bytes) of data
> to be copied from user space into the kernel.  What is the least
> and the greatest possible number of inspections of the page table
> (e.g. calls to pagedir_get_page()) that might result?  What about
> for a system call that only copies 2 bytes of data?  Is there room
> for improvement in these numbers, and how much?

A system call causing a full page of data to be copied from user space to the kernel should only happen if the user passes a buffer that is the size of an entire page to the `write` system call. All system calls take at most three arguments, all of which are pointers to other values. In some system calls - `open`, `create`, `exec`, `read`, and `write` - they may be pointers to character pointers. We need to check the address of each of these character pointers. In `open`, `create`, and `exec`, the number of character pointers must be 14 or less, since the strings represent file names, which are limited in length. So, `read` and `write` are the only two system calls that could have a full page of data be passed in by the user, and only `write` would have a full page of user data be copied to the kernel.

In the `write` system call, we check the address of `size` number of entries in the array of character pointers that makes up the `buffer` argument. Before we call `file_write()`, we run through a for loop for `size` iterations that calls `check_address()` to make sure that all of the data to be written to the file is valid. Each call to `check_address()` also contains a for loop that iterates 4 times to make sure that each byte in a given address is valid. So, if we had a buffer containing 4096 bytes to write to a file, we would have, at most, 4096*4 = 16384 calls to `pagedir_get_page()` if every single address turned out to be valid. This does not include calls to `check_address()` that also happen regardless, such as checking the system call ID the address of the pointer to the buffer. However, this number could be significantly lower - as low as 0, if the user passed a `NULL` pointer to the system call - if the very first address we checked was invalid.  The situation in which all addresses are valid results in a lot of inspections of the page table, but it does not seem as though we can improve this number in our code, as we do need to make sure that every address that the user process tries to use with a system call is valid. We cannot just use `pagedir_get_page()` to check the page that the buffer starts on, since the data may not be aligned on a page boundary and may span multiple pages, and we don't know where in the data that might happen until we check every address.

If a system call were to copy 2 bytes of data from user space to kernel using `write`, we would call `pagedir_get_page()`, at most, 2 * 4 = 8 times just to check the addresses of those bytes. Again, if the very first address we checked was invalid, we may not need any calls to find out. There is again not an apparent way to improve these numbers, since we need to check the address of each byte to be copied.

> B5: Briefly describe your implementation of the "wait" system call
> and how it interacts with process termination.

Our `wait` system call retrieves the process ID to wait on, then calls the `process_wait()` function on that process ID. `process_wait()` first verifies that the child thread to wait on is alive. If it isn't, then the parent process should not wait, so we check `thread_list` to see if the child's `thread_elem` is present in the list. If it isn't, we return -1, because the parent process has already waited on the child or the child never existed in the first place. If it is present, we retrieve and return its `exit_status` and then remove its `thread_elem` from `thread_list` to prevent the parent from waiting on it later.

If the thread associated with the `child_tid` argument is alive, then the parent process calls `sema_down()` on the child's `process_sema` field. The child calls `sema_up()` on this semaphore immediately before it exits, regardless of whether it exited normally or was killed. This unblocks the parent thread immediately, even if the child is deallocated before the parent thread resumes execution. When the parent starts again, it retrieves the child's `exit_status` from its `thread_elem` (which was not a field of the child's `thread` struct and thus was not deallocated with the child), removes the child's `thread_elem` from `thread_list`, frees the child's `thread_elem`'s `malloc`'ed memory, and then returns its exit status. We set the child's exit status to -1 elsewhere if it is killed, so `process_wait()` does not need to check for any extra cases.

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

We check all addresses passed into system calls, including the address of the argument passed to the system call handler, in a function called `check_address()`. This function forces a thread to exit if the address passed in is null, if it is a kernel virtual address (i.e. a user program should not be allowed to access it), or if it points to unmapped user virtual memory. If a system call takes a pointer to a character array as an argument, we also call `check_address()` in a loop to check each pointer in that array. This happens in system calls `open`, `read`, and `write`. Even in cases where we have to check the address of three arguments, plus use a loop to check character array pointers, the error checking process is not too obtrusive. We also separate the sections of code that pop arguments off the stack and check the addresses of those arguments from the sections of code that actually implement the system calls, which makes it easier to understand the actual functionality of the calls.

If a process does try to pass a bad pointer to a system call, we call `thread_exit()` to kill the thread. This function naturally leads to thread deallocation as part of the thread scheduling process. In `thread_exit()`, we go through the dying thread's list of open files and close each one and free any memory associated with the `fd_elem` that stored information about that file. We also call a function called `release_locks()` that iterates through the locks held by the current thread (kept track of in its `locks` list field) and releases each one. This ensures that any exiting thread releases all of its locks before exiting and that no other threads waiting on a lock held by a dying thread are stuck forever.

For example, let's say that a program has an open file that it is trying to write to, but the programmer accidentally passes a `NULL` pointer as the buffer instead of a buffer containing data to write into the file. Our `check_address()` function will catch the `NULL` pointer and will terminate the process, setting the process' exit status to -1 and calling `thread_exit()`. `thread_exit()` will close the file that the process had been trying to write to, since it's still open, and will release any locks that the process might have held.


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

If P calls `wait(C)` before C exits, P will call `sema_down()` on C's `process_sema` semaphore. C will call `sema_up()` on that semaphore when it exits, unblocking P immediately before C is set to die and be descheduled. This ensures that P does not run until C exits. Before calling `sema_down()`, P removes C from `thread_list` to prevent a strange case where the P tries to wait on C again before finishing `process_wait()` for the first time. After waking back up, P acquires C's exit status and frees the memory allocated for C's `thread_elem`.

If P calls `wait(C)` after C exits, the call to `get_thread_all(child_tid)` in `process_wait()` will return `NULL`, since there is no thread in `all_list` with thread ID `child_tid`. If this happens, we enter a for loop that iterates through all of the threads in `thread_list`, which has a node for each thread (dead or alive) whose parent has not waited on it yet. If we find a node with thread ID `child_tid` in this list, we remove it from the list, deallocate it, and return its exit status without waiting. If we don't find it, we return -1, since either the thread we are trying to wait on never existed or has already been waited on.

In both of the above cases, we remove C's `thread_elem` from `thread_list` as soon as P successfully waits on C. We do this removal in a lock that prevents any other thread from editing C's `thread_elem` to prevent race conditions. As soon as we are done with the contents of the `thread_elem`, we deallocate them using `free()` to prevent memory leaks.

Whenever a thread exits, all of the memory associated with is deallocated except for that associated with its `thread_elem`. This is necessary, because we need to keep the information associated with a C around after it terminates in case P tries to wait on it later. However, if P exits before waiting on C, it is not possible for P to wait on C.

If C hasn't exited yet, we want to keep its `thread_elem` around, because it continues to update its `thread_elem` until it dies and deallocating the structure before the thread exits would cause errors. However, once C and P have both exited, we have no reason to keep C's `thread_elem` around. So, in our loop that determines which (if any) `thread_elem`s in `thread_list` are associated with the `child_tid` argument, we also check each entry in the list to see if both the thread it's associated with and its parent are null. If they are, then we remove that `thread_elem` from the list and deallocate it, as we no longer need to access it or any information about it.

Each time we access or change any fields of a `thread_elem` (in `process_wait()` and anywhere else), we acquire the `thread_elem`'s `lock`. This ensures that we prevent race conditions that may occur when both a thread and its parent are looking and/or changing the same field of the same `thread_elem`.

#### RATIONALE

> B9: Why did you choose to implement access to user memory from the
> kernel in the way that you did?

We chose to implement our argument-validating `check_address()` function using functions from userprog/pagedir.c and threads/vaddr.h because it was relatively simple, compared to the other approach presented in the assignment. While our approach likely requires a bit more code inside the system call handler, and is less efficient than the other approach, the calls to `check_address()` are not particularly obtrusive and are separate from the functions that actually implement the system calls. Our approach was also quick to write and debug.

The system calls that access the file system work with the provided list library, and use provided file system functions to actually access the files. By mostly using provided resources, we decreased the chance of mistakes that could cause our system calls to let processes execute illegal instructions or access illegal memory.

> B10: What advantages or disadvantages can you see to your design
> for file descriptors?

The file descriptors that we implemented is based on the doubly-linked list class in the kernel library. Every thread has a list of open file descriptor that keep track of all files opened by this thread. The advantage of this implementation is that since these file descriptor lists are separated under each thread, no thread can mess with files opened by other threads. The disadvantage might be that there will be some slight memory overhead whenever we want to access file since we are using doubly-linked list for every thread in the system. We also have to traverse a thread's list of files each time we make a system call that accesses the file system, which may slow down these calls, especially for threads that are accessing lots of files.

> B11: The default tid_t to pid_t mapping is the identity mapping.
> If you changed it, what advantages are there to your approach?

N/A - we did not change the mapping.
