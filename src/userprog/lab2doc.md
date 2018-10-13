
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

***HOW DO WE AVOID OVERFLOWING THE STACK PAGE***

#### RATIONALE/JUSTIFICATION

> A3: Why does Pintos implement strtok_r() but not strtok()?

According to the `strtok()` man page, `strtok_r()` is thread-safe but `strtok()` is not. We may have multiple threads trying to tokenize strings at the same time, so we need to use a version that is thread-safe.

> A4: In Pintos, the kernel separates commands into a executable name
> and arguments.  In Unix-like systems, the shell does this
> separation.  Identify at least two advantages of the Unix approach.

### SYSTEM CALLS

#### DATA STRUCTURES

> B1: Copy here the declaration of each new or changed `struct` or
> `struct` member, `global` or `static` variable, `typedef`, or
> enumeration.  Document the purpose of each in 25 words or less.

New fields in the thread struct:
- `struct list children;` - keeps track of which threads are direct children of this thread.
- `struct thread* parent;` - keeps track of which thread created this thread.
- `struct semaphore process_sema;` - used with `process_wait()`. When a thread waits on a child thread, it calls `sema_down()` on the child's `process_sema`; when a child thread exits, it calls `sema_up()` on it.
- `bool process_waiting;` - keeps track of whether the child's parent thread has called `process_wait()` on it yet.
- `bool thread_killed;` - saves how the thread died (naturally or being killed by `kill()`).
- `int exit_status;` - saves the thread's exit status (i.e. what its process returned when the user program exited).
- `bool success;` - saves whether a `load()` done in this thread was successful or not.

Some of these fields are primarily important AFTER a thread has died. Before a thread dies, these fields are saved into a `dead_elem` item in the `dead_threads` list so that we retain access to them after the thread is deallocated. They are only saved by the thread itself so that we have easy access to all of them in `thread_exit()`.

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

Structs in thread.h:
```
struct tid_elem
  {
    struct list_elem elem;
    tid_t tid;
    int exit_status;
  };
```
The `tid_elem` structure is associated with a thread's `children` list. Each of the `list_elem`s in this list is part of a `tid_elem` struct. We store the TID and exit_status so that we have access to them where we need it, like in `process_wait()`.

- `struct list dead_threads;`
Stores a list of threads that have died. If we have a lot of threads, this list could potentially cause problems, since we never deallocate information about these threads. However, we do need to keep track of threads that have died (and how they died) to use in `process_wait()`. ***THIS COULD POTENTIALLY BE COMBINED WITH THE CHILD LIST OF A THREAD??***

```
struct dead_elem
  {
    struct list_elem elem;
    tid_t tid;
    bool killed;
    bool success;
    int exit_status;
  };
```
Contains the `list_elem`s that we put in `dead_threads`. Saves the information that goes with each node in `dead_threads`.

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

Our `sys_wait()` function, which is called by the system call handler when a user process invokes the `wait` system call, simply retrieves the argument to the call and passes it in to `process_wait()`. The `process_wait()` function then takes care of actual waiting, special cases, etc. as its functionality is identical to the `wait` system call's.

A lot happens in `process_wait()`. The main idea is that it takes a thread ID, checks to makes sure it is associated with a valid child thread to wait on, and then calls `sema_down()` on that child thread's `process_sema` semaphore field. This blocks the calling parent thread until the child exits and calls `sema_up()` on its own `process_sema`. We then check to ensure that the child exited in a valid way, and retrieve and return its exit status. The child thread and its `process_sema` field disappear shortly after calling `sema_up()` on it, but this does not cause issues because the parent thread is unblocked immediately and we save off all important information about the thread elsewhere before calling `sema_up()`.

Since `process_wait()` forces a parent process to wait until the child terminates, it is inherently related to how processes exit. Specifically, a parent thread needs to have access to a lot of information about how a child thread exited, including 1) whether it was killed by `kill()` or exited normally, 2) whether it failed to load the executable to run or not, and 3) what the child's exit status was. We have two ways of storing this information.

The first is in each thread's `children` list. This list stores information about the children that a thread spawned - specifically, their thread IDs and their exit statuses. We update the exit statuses associated with each node in this list when the child thread dies. The main purpose of this list is to allow us to find a dead child thread's exit status, as this information isn't otherwise accessible within `process_wait()`.

The second way that we save information about dead threads is in the global `dead_threads` list. This list uses the `dead_elem` struct to store more information about dead threads, including how they died (killed by `kill()`, stopped due to failed `load()`, exit status) along with their thread IDs. We use this list to 1) allow a parent to return information about a dead child that it hasn't waited on yet and 2) determine how a child died. Again, we push the nodes with this information onto the list right before a thread dies to ensure that the information is, in fact, saved and accessible where we need it.

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
