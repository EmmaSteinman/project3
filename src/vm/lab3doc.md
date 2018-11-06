
# Project 3 Design Document

> Denison cs372  
> Fall 2018

## GROUP

> Fill in the name and email addresses of your group members.

- Hayley LeBlanc <leblan_h1@denison.edu>
- Emma Steinman <steinm_e1@denison.edu>

## PRELIMINARIES

> If you have any preliminary comments on your submission, notes for the
> Instructor, or extra credit, please give them here.

> Please cite any offline or online sources you consulted while
> preparing your submission, other than the Pintos documentation, course
> text, lecture notes, and course staff.

## PROJECT PARTS

> In the documentation that you provide in the following, you need to provide clear, organized, and coherent documentation, not just short, incomplete, or vague "answers to the questions".  The questions are provided to help you structure your thinking around the information you need to convey.

### A. PAGE TABLE MANAGEMENT  

#### DATA STRUCTURES

> A1: Copy here the declaration of each new or changed `struct` or
> `struct` member, `global` or `static` variable, `typedef`, or
> enumeration.  Document the purpose of each in 25 words or less.

In palloc.h:

    struct frame_entry
      {
        struct thread* t;
      };

The `frame_entry` struct represents an entry in the frame table. The frame table holds pointers to `frame_entry` structs, which contain the information associated with that entry in the frame table.

- `void * allocate_page (enum palloc_flags flags);`: a function that replaces `palloc_get_page()` when the kernel needs to allocate a page for a user process. Uses `palloc_get_page()` to allocate a page, then uses that page to create a new entry in the frame table.

In thread.h:

```  
  struct page_table_elem
    {
      struct hash_elem elem;
      void* addr;
      int page_no;
      struct thread* t;
      struct file* file;
      char** name;
      bool writable;
      size_t page_read_bytes;
      size_t page_zero_bytes;
      int ofs;
      int pos;
    };
```

A `page_table_elem` is an element of the supplemental page table. It contains all of the necessary information about an entry in the page table and is used to determine what data should be loaded and where it should be put when a page fault occurs due to data not being present.

In the `thread` struct:

- `struct hash s_page_table;`: the supplemental page table.
- `struct lock spt_lock;`: a lock used with the supplemental page table to prevent race conditions when we access and change entries of the table.

Functions:
- `void check_page (void* addr)` in syscall.h. Checks that the address of a buffer we are trying to read data into is not in an unwritable page.

#### ALGORITHMS

> A2: In a few paragraphs, describe your code for accessing the data
> stored in the SPT about a given page.

**write more on this later**
- entries are stored in a hash table
- there is a supplemental page table for each thread
- we hash by page number + thread struct pointer; this reduces collisions
- when we need to access the SPT, we can just create a `page_table_elem` with the page number and thread pointer that we want, and this will provide all of the information that was previously saved about this entry and will let us load it

*need to add more here when swapping and stuff is implemented*

> A3: How does your code coordinate accessed and dirty bits between
> kernel and user virtual addresses that alias a single frame, or other aliasing that might occur through sharing?

#### SYNCHRONIZATION

> A4: When two user processes both need a new frame at the same time,
> how are races avoided?

We use a lock, `alloc_lock`, in palloc.c to prevent race conditions. User processes should only try to allocate a page via our `allocate_page()` function, so this lock goes around the code in this function. This ensures that two user processes cannot execute `allocate_page()` concurrently and thus there will be no race conditions surrounding page/frame acquisition.

We do *not* use `alloc_lock` in situations where the kernel is getting a new frame. There will not be any races between user and kernel processes when acquiring frames because they do not share page pools.


#### RATIONALE

> A5: Why did you choose the data structure(s) that you did for
> representing virtual-to-physical mappings?

Our frame table, which keeps track of which frames are in use, is an array. This is not the most space efficient representation, as it requires a slot in the array even for frames that are not in use. We could potentially make this more efficient by using a multi-level frame table like the page directories that we discussed in class. However, each slot in the array only holds a 4-byte pointer to a `frame_entry` struct, so we do not have to allocate much memory for unused frames. We chose to use an array because it is much more time efficient to find a specific frame entry in an array than it is in a list. We index into the frame table using the PFN of an address, so in order to access the frame table entry corresponding to an address, we just need to convert it to a physical address and extract its frame number. This process only requires some simple bit manipulations and is thus significantly faster than alternatives like iterating through a list to find the entry we want. Also, the time required to access an entry in the frame table stays constant regardless of how many frames are in use, which is not true of a list. The space overhead of our array approach and a list-based approach also become closer as more and more frames come into use.

We used hash tables to implement our supplemental page table. Each thread has a hash table that contains supplemental page table entries that are hashed by VPN. Different threads may try to place pages in the same virtual addresses in their distinct address spaces, so keeping the SPTs separate for each thread prevents collisions and makes it easy to free resources when a thread exits. Like our array approach to the frame table, hash tables are very time efficient and allow us to quickly access entries. The provided hash table implementation also provides functions that make it very easy to insert values into a hash table and to delete a hash table, two important operations with our SPTs. It also takes care of space management and rehashing, which makes our code easier to write, debug, and understand.

### B. PAGING TO AND FROM DISK
#### DATA STRUCTURES

> B1: Copy here the declaration of each new or changed `struct` or
> `struct` member, global or static variable, `typedef`, or
> enumeration.  Identify the purpose of each in 25 words or less.

We created a new set of files, page.h and page.c, in the vm directory to contain functions that are related to paging. The following are defined in these files:

- `STACK_SIZE`: a constant that defines the maximum number of pages allowed to be allocated for a single process's stack.
- `void add_stack_page (struct intr_frame *f, void* addr);`: a function that, given an interrupt frame and an address (either an address being check for use with a system call or an address that caused a page fault), allocates a new page for the current process's stack.
- `bool install_page (void *upage, void *kpage, bool writable);`: a function copied from process.c that puts a new page in a process's page directory. Was previously not publicly available, so we copied it to page.c so that it can be used in other files.


#### ALGORITHMS ####

> B2: When a frame is required but none is free, some frame must be
> evicted.  Describe your code for choosing a frame to evict.

> B3: When a process P obtains a frame that was previously used by a
> process Q, how do you adjust the page table (and any other data
> structures) to reflect the frame Q no longer has?

> B4: Explain your heuristic for deciding whether a page fault for an
> invalid virtual address should cause the stack to be extended into
> the page that faulted.

We check three things about an invalid address to see if it should cause stack growth. First, we check that the difference between the stack pointer and the invalid address is less than or equal to 32 bytes. If it is, this implies that the instruction that caused the fault was a `push` or `pusha` instruction, so we should extend the stack. The second thing we check is whether the difference is greater than -100000. When we are *not* trying to put data onto the stack, the difference between the stack pointer and the invalid address tends to be a very small negative number, i.e. the difference is millions of bytes. In some cases where we are trying to put a large amount of data onto the stack, the difference will still be a negative number, but it will be much closer to zero. Based on the provided tests, -100000 seems to be a good number that non-stack accesses will be smaller than but stack accesses will be larger than. Finally, we check that the invalid address and the stack pointer are not the same. This only comes into play when we are checking system call addresses, but we don't want to add a page for the stack if the stack pointer itself is invalid; that means that some other problem has occurred.

#### SYNCHRONIZATION ####

> B5: Explain the basics of your VM synchronization design.  In
> particular, explain how it prevents deadlock.  (Refer to the
> textbook for an explanation of the necessary conditions for
> deadlock.)

> B6: A page fault in process P can cause another process Q's frame
> to be evicted.  How do you ensure that Q cannot access or modify
> the page during the eviction process?  How do you avoid a race
> between P evicting Q's frame and Q faulting the page back in?

> B7: Suppose a page fault in process P causes a page to be read from
> the file system or swap.  How do you ensure that a second process Q
> cannot interfere by e.g. attempting to evict the frame while it is
> still being read in?

> B8: Explain how you handle access to paged-out pages that occur
> during system calls.  Do you use page faults to bring in pages (as
> in user programs), or do you have a mechanism for "locking" frames
> into physical memory, or do you use some other design?  How do you
> gracefully handle attempted accesses to invalid virtual addresses?

#### RATIONALE ####

> B9: A single lock for the whole VM system would make
> synchronization easy, but limit parallelism.  On the other hand,
> using many locks complicates synchronization and raises the
> possibility for deadlock but allows for high parallelism.  Explain
> where your design falls along this continuum and why you chose to
> design it this way.

### C. MEMORY MAPPED FILES

#### DATA STRUCTURES ####

> C1: Copy here the declaration of each new or changed `struct` or
> `struct` member, global or static variable, `typedef`, or
> enumeration.  Identify the purpose of each in 25 words or less.

#### ALGORITHMS ####

> C2: Describe how memory mapped files integrate into your virtual
> memory subsystem.  Explain how the page fault and eviction
> processes differ between swap pages and other pages.

> C3: Explain how you determine whether a new file mapping overlaps
> any existing segment.

#### RATIONALE ####

> C4: Mappings created with "mmap" have similar semantics to those of
> data demand-paged from executables, except that "mmap" mappings are
> written back to their original files, not to swap.  This implies
> that much of their implementation can be shared.  Explain why your
> implementation either does or does not share much of the code for
> the two situations.
