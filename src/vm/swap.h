#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <list.h>
#include <bitmap.h>
#include "devices/disk.h"
#include "threads/thread.h"

void swap_init (void);
void swap_allocate (uint8_t *kpage, uint8_t *upage, struct thread *t);
void swap_free (uint8_t *upage, struct thread *t);
bool swap_in_table (uint8_t *upage, struct thread *t);

#endif /* vm/swap.h */
