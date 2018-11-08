#include "vm/frame.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/thread.h"

// NEW
/* Replaces calls to palloc_get_page(). Uses palloc_get_page() to
   allocate a page, and adds an entry to the frame table about
   that page. */
void *
allocate_page (enum palloc_flags flags)
{
  lock_acquire (&alloc_lock);
  void* va_ptr;
  va_ptr = palloc_get_page(flags);
  // if this returns null, then we need to swap out a page
  // this has to be done here otherwise we will fail in vtop
  if (va_ptr == NULL)
    va_ptr = swap_out();

  // this is a VIRTUAL ADDRESS, so to get the frame number, we need
  // to translate it
  uintptr_t phys_ptr = vtop (va_ptr);
  uintptr_t pfn = pg_no (phys_ptr);

  // initialize the new frame table entry
  struct frame_entry* entry = malloc(sizeof(struct frame_entry));
  entry->t = thread_current();
  entry->va_ptr = va_ptr;

  // add the entry to the frame table at the index of the PFN
  frame_table[pfn-625] = entry; // normalize so that indexing starts at 0
  lock_release (&alloc_lock);

  return va_ptr;
}