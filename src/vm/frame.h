#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/palloc.h"
#include "vm/page.h"

/* Frame table entry */
struct frame
{
    uint8_t *kpage;
    struct page *page;
    struct list_elem elem;
};

void frame_init (void);
void *frame_alloc (enum palloc_flags flags, void *upage, bool writable);
void *frame_alloc_with_page (enum palloc_flags flags, struct page *p);
void frame_free (void *kpage);
void *frame_evict (void);

#endif
