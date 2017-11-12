#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <list.h>
#include "threads/thread.h"
#include "threads/interrupt.h"
#include "filesys/file.h"
#include "filesys/off_t.h"

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
  bool pin;

  bool load; // lazy load
  struct load_info *load_info;

  struct list_elem elem;
};

struct load_info
{
  struct file *file;
  off_t ofs;
  uint32_t read_bytes;
  uint32_t zero_bytes;
};

// TODO: thread -> tid로 바꾸자: memory 효율

void page_insert (uint8_t *upage, struct thread *t, bool writable);
void page_remove (uint8_t *upage, struct thread *t);

enum page_status page_status (uint8_t *upage, struct thread *t);
void page_change_status (uint8_t *upage, struct thread *t, enum page_status status);
struct page * page_find (uint8_t *upage, struct thread *t);

bool stack_growth (struct intr_frame *f UNUSED, uint8_t *upage, struct thread *t);
bool page_stack_growth (uint8_t *upage, struct thread *t);

void page_init (void);
void page_set_file(struct file *file, uint8_t *upage, off_t ofs, uint32_t page_read_bytes, uint32_t page_zero_bytes, bool writable);
bool page_load_file (uint8_t *upage);

#endif /* vm/page.h */
