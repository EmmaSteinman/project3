#ifndef SWAP_H
#define SWAP_H

#include <bitmap.h>
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <list.h>
#include <bitmap.h>
#include "threads/palloc.h"
#include "vm/page.h"

struct swap_table_elem
  {
    struct list_elem elem;
    void* va_ptr;
    int swap_location;
    struct page_table_elem* spte;
  };

struct block* swap_block;
int num_swap_slots;

struct bitmap* swap_slots;

void swap_init ();
void* swap_out ();
void swap_in (void* addr, struct page_table_elem* spte);

#endif // SWAP_H
