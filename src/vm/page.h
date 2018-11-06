#ifndef PAGE_H
#define PAGE_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"

#define STACK_SIZE 32 // each process is allowed 32 pages of stack (this is arbitrary and we can change it)

void add_stack_page (struct intr_frame *f, void* addr);
bool install_page (void *upage, void *kpage, bool writable);

#endif
