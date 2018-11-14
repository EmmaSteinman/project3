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
    int swap_location;
  };

struct block* swap_block;
int num_swap_slots;

struct lock swap_lock;

struct bitmap* swap_slots;

int current_clock;
void swap_init (void);
void* swap_out (void);
void swap_in (uint8_t* addr, struct page_table_elem* spte);

#endif // SWAP_H
