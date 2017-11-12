#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <list.h>
#include <bitmap.h>
#include "devices/disk.h"
#include "threads/thread.h"

void swap_init (void);
void swap_out (uint8_t *kpage, uint8_t *upage, struct thread *t);
void swap_in (uint8_t *upage, uint8_t *kpage, struct thread *t);

#endif /* vm/swap.h */
