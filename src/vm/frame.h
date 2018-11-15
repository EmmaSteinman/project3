#ifndef FRAME_H
#define FRAME_H

#include "threads/palloc.h"
#include "vm/page.h"

// struct that holds information about entries in the frame table
struct frame_entry
  {
    struct thread* t;
    void* va_ptr; // the kernel VA associated with this frame
    struct page_table_elem* spte;
    bool pinned;
    int reference;
  };

struct frame_entry** frame_table;
int user_pgs;

struct lock alloc_lock;
struct lock frame_lock;

void * allocate_page (enum palloc_flags flags);
void free_page (void* page);

#endif
