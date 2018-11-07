#ifndef SWAP_H
#define SWAP_H


struct swap_table_elem
  {
    struct thread* owner;
    int page_no;
  };

struct swap_table_elem* swap_table;

void swap_init ();

#endif // SWAP_H
