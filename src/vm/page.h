#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <list.h>
#include "threads/thread.h"
#include "threads/interrupt.h"

/* Where is page */
enum page_status
  {
    PAGE_FRAME,   
    PAGE_FILE,    /* in the file system */
    PAGE_SWAP,    /* in a swap slot */
    PAGE_ERROR
  };

struct page
{
  uint8_t *upage; /* logical address: page */
  struct thread *thread;
  enum page_status status;
  bool writable;

  struct list_elem elem;
};

// TODO: thread -> tid로 바꾸자: memory 효율

void page_insert (uint8_t *upage, struct thread *t, bool writable);
void page_remove (uint8_t *upage, struct thread *t);

enum page_status page_status (uint8_t *upage, struct thread *t);
void page_change_status (uint8_t *upage, struct thread *t, enum page_status status);
struct page * page_find (uint8_t *upage, struct thread *t);

bool stack_growth (struct intr_frame *f UNUSED, uint8_t *upage, struct thread *t);

void page_init (void);
void page_stack_growth (void *fault_addr, void **esp);

#endif /* vm/page.h */