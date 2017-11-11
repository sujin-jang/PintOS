#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "threads/thread.h"
#include "threads/palloc.h"

void frame_init (void);
void frame_insert (uint8_t *kpage, uint8_t *upage, struct thread *t);
void frame_eviction (void);

void * palloc_get_page_with_frame (enum palloc_flags flags, uint8_t *upage, bool writable);
void palloc_free_page_with_frame (uint8_t *upage);

#endif /* vm/frame.h */