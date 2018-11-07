#include "vm/swap.h"
#include "devices/block.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"

void swap_init (void)
{
  struct block* swap_block = block_get_role(BLOCK_SWAP); // get a pointer to the location of the swap disk
  int num_swap_slots = (block_size(swap_block)*BLOCK_SECTOR_SIZE) / PGSIZE; // get the number of swap slots on the swap disk
  swap_table = malloc(sizeof(struct swap_table_elem)*num_swap_slots); // allocate an array for the swap table
}
