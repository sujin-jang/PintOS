#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "vm/frame.h"

void swap_out (struct frame *frame);
bool swap_in (void *kpage, struct page *p);
void swap_free (void);

#endif
