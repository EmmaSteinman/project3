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

void swap_init (void)
{
  swap_block = block_get_role(BLOCK_SWAP); // get a pointer to the location of the swap disk
  num_swap_slots = (block_size(swap_block)*BLOCK_SECTOR_SIZE) / PGSIZE; // get the number of swap slots on the swap disk
  swap_slots = bitmap_create(num_swap_slots);
}

void* swap_out ()
{
  // this section of code randomly selects a frame to remove
  // it is NOT very good but should work for now
  // might be sometimes causing us to lose the name of the file???
  // there is still more work to do with restricting which frames can be evicted
  // and what we can and cannot edit during an eviction
  int frame = 0;
  struct frame_entry* frame_ptr = NULL;
  while (frame_ptr == NULL)
  {
    frame = (random_ulong() % user_pgs) + 1;
    frame_ptr = frame_table[frame];
  }
  void* va_ptr = frame_ptr->va_ptr;

  // if the frame is dirty we have to write it to swap
  if (pagedir_is_dirty(frame_ptr->t->pagedir, frame_ptr->va_ptr) && frame_ptr->spte->writable)
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
    frame_ptr->spte->swap_elem = s;
    list_push_back(&frame_ptr->t->swap_table, &s->elem);
    frame_ptr->spte->swapped = true;
  }
  // otherwise, we can just clear the page

  // free the resources in this page and the frame pointer so that we can put something else here
  // TODO: are there other resources that we may need to free?
  pagedir_clear_page (frame_ptr->t->pagedir, frame_ptr->spte->addr);
  free (frame_ptr);
  return va_ptr;
}

void swap_in (void* addr, struct page_table_elem* spte)
{
  struct thread* cur = thread_current();

  int swap_loc = spte->swap_elem->swap_location;

  // get a page (possibly swapping something else out)
  // this also creates a new entry in the frame table for this frame
  uint8_t* kpage = allocate_page (PAL_USER);

  // read the data in the swap slot into the new page
  int i;
  for (i = 0; i < 8; i++)
  {
    block_read (swap_block, (swap_loc * 8) + i, kpage + (512 * i));
  }

  // set the bitmap slot that corresponds to this swap slot to 0
  // so that we can put something else in it
  bitmap_set (swap_slots, swap_loc, 0);

  // install this page in the page directory
  if (!install_new_page (pg_round_down(addr), kpage, spte->writable))
    {
      palloc_free_page (kpage);
      lock_acquire(&cur->element->lock);
      cur->element->exit_status = -1;
      lock_release(&cur->element->lock);
      thread_exit();
    }

  // set the entry in the frame table to correspond to this supplemental page table entry
  uintptr_t phys_ptr = vtop (kpage);
  uintptr_t pfn = pg_no (phys_ptr);
  frame_table[pfn-625]->spte = spte;

  // TODO: free swap table entry corresponding to this?
}
