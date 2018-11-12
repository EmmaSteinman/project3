
#include "vm/page.h"
#include <stdio.h>
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/frame.h"


/* Adds a page to the current thread's stack. Checks whether adding a page will
   make the thread's stack too big, allocates a new page for it, sets that page
   to zeroes, installs the page in the thread's page directory, and adds it to
   the current thread's SPT. Called in syscall.c by check_address() and in
   the page fault handler.
*/
void add_stack_page (struct intr_frame *f, void* addr)
{
  struct thread* cur = thread_current();
  if (cur->stack_pages >= STACK_SIZE)
  {
    lock_acquire(&cur->element->lock);
    cur->element->exit_status = -1;
    lock_release(&cur->element->lock);
    thread_exit();
  }
  uint8_t *kpage = allocate_page (PAL_ZERO);

  if (kpage == NULL)
  {
    lock_acquire(&cur->element->lock);
    cur->element->exit_status = -1;
    lock_release(&cur->element->lock);
    thread_exit();
  }

  uintptr_t phys_ptr = vtop (kpage);
  uintptr_t pfn = pg_no (phys_ptr);
  lock_acquire (&frame_lock);
  frame_table[pfn-625]->pinned = true;
  lock_release (&frame_lock);

  // set the page to 0
  memset (kpage, 0, 4096);
  // install our page in the page directory so that it is writable
  if (!install_new_page (pg_round_down(addr), kpage, true))
    {
      palloc_free_page (kpage);
      lock_acquire(&cur->element->lock);
      cur->element->exit_status = -1;
      lock_release(&cur->element->lock);
      thread_exit();
    }

  // add this page to the SPT
  struct page_table_elem* entry = malloc(sizeof(struct page_table_elem));
  entry->t = cur;
  entry->page_no = pg_no(addr);
  entry->writable = true;
  entry->swapped = false;

  lock_acquire (&cur->spt_lock);
  struct hash_elem* h = hash_insert (&cur->s_page_table, &entry->elem);
  lock_release (&cur->spt_lock);

  cur->stack_pages++;

  // associate kpage's frame table entry with this SPTE
  lock_acquire (&frame_lock);
  frame_table[pfn-625]->spte = entry;
  entry->frame_ptr = frame_table[pfn-625];
  frame_table[pfn-625]->pinned = false;
  lock_release (&frame_lock);
}

/* Adds a new page from disk (not swap) based on an SPT entry. */
void
add_spt_page (struct intr_frame *f, void *addr)
{
  struct hash_elem* e;
  struct page_table_elem p;
  struct thread* cur = thread_current();

  p.page_no = pg_no (addr);
  p.t = cur;

  lock_acquire (&cur->spt_lock);
  e = hash_find (&cur->s_page_table, &p.elem);
  lock_release (&cur->spt_lock);
  if (e == NULL)
  {
    // printf("KILLING THREAD\n");
    // printf("addr: %x\n", addr);
    // printf("instruction ptr: %x\n", f->eip);
    lock_acquire(&cur->element->lock);
    cur->element->exit_status = -1;
    lock_release(&cur->element->lock);
    thread_exit();
  }

  lock_acquire (&cur->spt_lock);
  struct page_table_elem* entry = hash_entry(e, struct page_table_elem, elem);
  lock_release (&cur->spt_lock);
  if (entry == NULL)
  {
    lock_acquire(&cur->element->lock);
    cur->element->exit_status = -1;
    lock_release(&cur->element->lock);
    thread_exit();
  }

  uint8_t *kpage = allocate_page (PAL_ZERO);

  if (kpage == NULL)
  {
    lock_acquire(&cur->element->lock);
    cur->element->exit_status = -1;
    lock_release(&cur->element->lock);
    thread_exit();
  }

  uintptr_t phys_ptr = vtop (kpage);
  uintptr_t pfn = pg_no (phys_ptr);
  lock_acquire (&frame_lock);
  frame_table[pfn-625]->pinned = true;
  lock_release (&frame_lock);

  if (entry->swapped == true)
  {
    // if this entry was swapped out, we need to swap it back from the swap device
    swap_in (kpage, entry);
  }
  else
  {
    // associate kpage's frame table entry with this SPTE
    // uintptr_t phys_ptr = vtop (kpage);
    // uintptr_t pfn = pg_no (phys_ptr);
    lock_acquire (&frame_lock);
    frame_table[pfn-625]->spte = entry;
    entry->frame_ptr = frame_table[pfn-625];
    lock_release(&frame_lock);

    // we should only open the file if we actually need to read bytes from it
    if (entry->page_read_bytes > 0)
    {
      // the thread that page faulted might have faulted while it held the file lock,
      // so we only need to acquire it if we don't already have it
      // TODO: this issue shows up a couple other places (faulting while we hold a resource that
      // we need to handle the fault). we need to come up with some way to deal with it that is probably
      // not this way.
      bool acquired_lock = false;
      if (!lock_held_by_current_thread(&file_lock))
      {
        acquired_lock = true;
        lock_acquire(&file_lock);
      }
      struct file* file = filesys_open(entry->name);
      file_seek (file, entry->pos + entry->ofs);
      if (file_read (file, kpage, entry->page_read_bytes) != entry->page_read_bytes)
        {
          if (acquired_lock == true)
            lock_release(&file_lock);
          palloc_free_page (kpage);
          lock_acquire(&cur->element->lock);
          cur->element->exit_status = -1;
          lock_release(&cur->element->lock);
          thread_exit();
        }
      file_close(file);
    if (acquired_lock == true)
      lock_release(&file_lock);
    }
    memset (kpage + entry->page_read_bytes, 0, entry->page_zero_bytes);

    if (!entry->writable)
      pagedir_set_dirty(cur->pagedir, kpage, false);
  }
  if (!install_new_page (entry->addr, kpage, entry->writable))
    {
      palloc_free_page (kpage);
      lock_acquire(&cur->element->lock);
      cur->element->exit_status = -1;
      lock_release(&cur->element->lock);
      thread_exit();
    }
  lock_acquire (&frame_lock);
  frame_table[pfn-625]->pinned = false;
  lock_release (&frame_lock);
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
bool
install_new_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
