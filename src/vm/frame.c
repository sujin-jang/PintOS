#include "vm/frame.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include <list.h>
#include <debug.h>

/* Frame table */
struct list frame_table;

/* Lock to protect frame table */
struct lock frame_lock;

struct lock evict_lock;

static struct frame * frame_find (void *kpage);

/* Initializes the frame table (called in threads/init.c) */
void
frame_init (void)
{
  list_init (&frame_table);
  lock_init (&frame_lock);

  lock_init (&evict_lock);
  return;
}

/* Frame allcoate */
void *
frame_alloc (enum palloc_flags flags, void *upage, bool writable)
{
  ASSERT(flags & PAL_USER); // if not PAL_USER, ASSERT
  ASSERT (pg_ofs (upage) == 0);

  /*
  1. free frame 존재하면: palloc_get_page -> page_allocate
  2. free frame 존재하지 않으면: eviction 후 palloc_get_page -> page_allocate
    3. 성공하면 page install
  4. frame table에 push
  */

  lock_acquire(&evict_lock);
  struct page *p = page_insert (upage, writable, PAGE_FRAME);
  ASSERT (p != NULL);

  uint8_t *kpage = palloc_get_page (flags);

  if (kpage == NULL)
    { 
      /* eviction */
      frame_evict ();
      kpage = palloc_get_page (flags); // evict 하고 온 사이에 다른애가 가로채면 어떡해 ?1! TODO: lock
      //return NULL;
    }
    
    struct frame *f = malloc (sizeof (*f));
    f->kpage = kpage;
    f->page = p;

    lock_acquire(&frame_lock);
    list_push_back (&frame_table, &f->elem);
    lock_release(&frame_lock);

    lock_release(&evict_lock);
  return kpage;
}

/* Frame allcoate */
void *
frame_alloc_with_page (enum palloc_flags flags, struct page *p)
{
  ASSERT(flags & PAL_USER); // if not PAL_USER, ASSERT

  uint8_t *kpage = palloc_get_page (flags);

  if (kpage == NULL)
    { 
      /* eviction */
      frame_evict ();
      kpage = palloc_get_page (flags); // evict 하고 온 사이에 다른애가 가로채면 어떡해 ?1! TODO: lock
      //return NULL;
    }
    
    struct frame *f = malloc (sizeof (*f));
    f->kpage = kpage;
    f->page = p;

    lock_acquire(&frame_lock);
    list_push_back (&frame_table, &f->elem);
    lock_release(&frame_lock);

  return kpage;
}


/* Frame free */
void
frame_free (void *kpage)
{
  /*
  1. find(kapge) != NULL 이면: palloc_free_page
  2. struct page의 status = not frame으로 변경..?
  3. frame table에서 pull
  4. find(kpage) == NULL 이면: nothing
  */

  struct frame *f = frame_find(kpage);
    if (f != NULL)
    {
      lock_acquire(&frame_lock);
      list_remove (&f->elem);
      lock_release(&frame_lock);
    }

  palloc_free_page (kpage);
    return;
}

static struct frame *
frame_find (void *kpage)
{
  lock_acquire(&frame_lock);
  struct list_elem *e;

  for (e = list_begin (&frame_table); e != list_end (&frame_table); e = list_next (e))
  {
    struct frame *f = list_entry (e, struct frame, elem);
    if (f->kpage == kpage)
    {
      lock_release(&frame_lock);
      return f;
    }
    }

    lock_release(&frame_lock);
    return NULL;
}

/* Choose a frame to evcit if run out of frames */
void *
frame_evict (void) // TODO: evict lock
{
  /* FIFO */

  struct list_elem *e = list_front (&frame_table);
  struct frame *f = list_entry (e, struct frame, elem);

  swap_out (f);

  f->page->status = PAGE_SWAP; // TODO: page lock
  
  pagedir_clear_page(f->page->thread->pagedir, f->page->upage); // TODO: pagedir lock
  frame_free (f->kpage);

    return NULL;
}
