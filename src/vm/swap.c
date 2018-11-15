#include "vm/swap.h"
#include <random.h>
#include <bitmap.h>
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "devices/block.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "vm/frame.h"
#include "userprog/pagedir.h"
#include "threads/palloc.h"
#include <random.h>
#include <string.h>

void swap_init (void)
{
  printf("swap\n");
  swap_block = block_get_role(BLOCK_SWAP); // get a pointer to the location of the swap disk
  num_swap_slots = (block_size(swap_block)*BLOCK_SECTOR_SIZE) / PGSIZE; // get the number of swap slots on the swap disk
  swap_slots = bitmap_create(num_swap_slots);
  current_clock = 1;

  lock_init (&swap_lock);
}

/* Finds a frame to evict and swaps it out, writing the contents of the
   frame to swap space if necessary. Clears the frame and returns a pointer
   to it so that a process can use it. */
void* swap_out ()
{
  lock_acquire (&swap_lock);

  struct frame_entry* frame_ptr;
  int found = 0;
  // iterate through the frame table until we find a frame to evict
  while (found != 1)
  {
    lock_acquire (&frame_lock);
    frame_ptr = frame_table[current_clock];

    if (frame_ptr != NULL && frame_ptr->spte != NULL && frame_ptr->pinned == false)
    {
      // if the frame hasn't been accessed recently, we have found a candidate for eviction
      if (!pagedir_is_accessed(frame_ptr->t->pagedir, frame_ptr->va_ptr) || !pagedir_is_accessed(frame_ptr->t->pagedir, frame_ptr->spte->addr))
      {
        found = 1;
      }
      // otherwise, update the accessed bits and move the clock
      else
      {
        pagedir_set_accessed(frame_ptr->t->pagedir, frame_ptr->va_ptr, false);
        pagedir_set_accessed(frame_ptr->t->pagedir, frame_ptr->spte->addr, false);
        //current_clock = (current_clock+1) % (user_pgs-1);
      }
    }
    current_clock = (current_clock+1) % (user_pgs-1);
    lock_release (&frame_lock);
  }
  void* va_ptr = frame_ptr->va_ptr;

  // clear the page here to prevent the owning process from editing this frame anymore
  bool dirty = pagedir_is_dirty(frame_ptr->t->pagedir, frame_ptr->va_ptr) || pagedir_is_dirty(frame_ptr->t->pagedir, frame_ptr->spte->addr);
  pagedir_clear_page (frame_ptr->t->pagedir, frame_ptr->spte->addr);

  // if the frame is dirty we have to write it to swap
  if (dirty && frame_ptr->spte->writable)
  {
    // find an empty swap slot (8 blocks, 1 bit in the bitmap)
    size_t open_slot = bitmap_scan_and_flip (swap_slots, 0, 1, 0);

    // now use that to write the page to disk (we need to write 8 sectors because there are 8 sectors in 1 page)
    int i;
    for (i = 0; i < 8; i++)
    {
      block_write (swap_block, open_slot * 8 + i, va_ptr + i * 512);
    }

    // create a swap table entry for this to save that it was swapped
    struct swap_table_elem* s = malloc(sizeof(struct swap_table_elem));
    s->swap_location = open_slot;
    lock_acquire (&frame_lock);
    frame_ptr->spte->swap_elem = s;
    list_push_back(&frame_ptr->t->swap_table, &s->elem);
    frame_ptr->spte->swapped = true;
    lock_release (&frame_lock);
  }

  // set the contents of the page to 0 so that the next thread that
  // obtains this page doesn't accidentally read old data that belonged
  // to the previous thread that held the page
  memset (va_ptr, 0, PGSIZE);

  // free the resources in this page and the frame pointer so that we can put something else here
  lock_acquire(&frame_lock);
  free (frame_ptr);
  lock_release(&frame_lock);
  lock_release (&swap_lock);
  return va_ptr;
}

void swap_in (uint8_t* kpage, struct page_table_elem* spte)
{
  lock_acquire (&swap_lock);
  struct thread* cur = thread_current();

  int swap_loc = spte->swap_elem->swap_location;

  // read the data in the swap slot into the new page
  int i;
  for (i = 0; i < 8; i++)
  {
    block_read (swap_block, (swap_loc * 8) + i, kpage + (512 * i));
  }

  // set the bitmap slot that corresponds to this swap slot to 0
  // so that we can put something else in it
  bitmap_set (swap_slots, swap_loc, 0);

  // now that this page has been swapped back in, set the swapped variable
  // with this SPTE back to false
  spte->swapped = false;

  // set the entry in the frame table to correspond to this supplemental page table entry
  uintptr_t phys_ptr = vtop (kpage);
  uintptr_t pfn = pg_no (phys_ptr);
  lock_acquire (&frame_lock);
  frame_table[pfn-625]->spte = spte;
  lock_release (&frame_lock);

  // remove this swap element
  list_remove (&spte->swap_elem->elem);
  free (spte->swap_elem);

  lock_release (&swap_lock);
}
