#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdbool.h>
#include <hash.h>
#include "threads/thread.h"

enum page_status
{
  PAGE_FRAME,
  PAGE_SWAP,
  PAGE_FILE,
  PAGE_MMAP
};

struct page
{
    uint8_t *upage;
    enum page_status status;
    struct thread *thread;

    int swap_index;
    bool writable;

    struct hash_elem elem;
};

struct page * page_insert (void *upage, bool writable, enum page_status status);
void page_remove (void *upage);
bool page_load (struct page *page);

struct page * page_find (struct hash page_table, void *upage);
bool page_stack_growth (void *upage);

void page_table_create (struct hash *page_table);
void page_table_destroy (struct hash *page_table);

#endif