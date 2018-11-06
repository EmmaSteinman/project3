#include "vm/page.h"
#include <stdio.h>
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"


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
  uint8_t *kpage = allocate_page (PAL_USER);
  if (kpage == NULL)
  {
    lock_acquire(&cur->element->lock);
    cur->element->exit_status = -1;
    lock_release(&cur->element->lock);
    thread_exit();
  }
  // set the page to 0
  memset (kpage, 0, 4096);
  // install our page in the page directory so that it is writable
  if (!install_page (pg_round_down(addr), kpage, true))
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

  lock_acquire (&cur->spt_lock);
  struct hash_elem* h = hash_insert (&cur->s_page_table, &entry->elem);
  lock_release (&cur->spt_lock);

  cur->stack_pages++;
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
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
