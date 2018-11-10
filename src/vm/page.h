
#ifndef PAGE_H
#define PAGE_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include <hash.h>
#include "threads/thread.h"
#include "threads/interrupt.h"

#define STACK_SIZE 32 // each process is allowed 32 pages of stack (this is arbitrary and we can change it)

/* To find an element in the supplemental page table, create one of these
   page_table_elem's and set its page number and t (thread) field. This
   is the information that we use to compute the hash of an entry. */
struct page_table_elem
  {
    struct hash_elem elem;  // element to go in the hash table
    void* addr;             // virtual address of the associated page
    int page_no;            // page number
    struct thread* t;       // thread associated with this entry
    struct file* file;      // is this used?
    char** name;            // name of the file associated with this entry
    bool writable;          // keeps track of whether this page is writable
    size_t page_read_bytes; // number of bytes to read into this page from the file
    size_t page_zero_bytes; // number of bytes to set to zero at the end of the page
    int ofs;                // offset to read from file
    int pos;                // position to read from file
    bool swapped;           // keeps track of whether the page has been swapped out
    struct swap_table_elem* swap_elem;
    struct frame_entry* frame_ptr;
  };

void add_stack_page (struct intr_frame *f, void *addr);
void add_spt_page (struct intr_frame *f, void *addr);
bool install_new_page (void *upage, void *kpage, bool writable);

#endif
