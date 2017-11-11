#include "vm/page.h"
#include "userprog/process.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include <stdio.h>

/* Supplemental Page Table
 * 1. page fault: kernel find out what data should be there
 * 2. process terminate: kernel decide what resources to free
 */

struct list page_list;
struct lock page_lock;
struct lock page_lock_2;

void
page_init (void) 
{
  list_init (&page_list);
  lock_init (&page_lock);
  lock_init (&page_lock_2);
}

void
page_insert (uint8_t *upage, struct thread *t, bool writable)
{
  lock_acquire (&page_lock);
  struct page *p = malloc (sizeof *p);
  p->upage = upage;
  p->thread = t;
  p->status = PAGE_FRAME;
  p->writable = writable;

  list_push_back (&page_list, &p->elem);
  lock_release (&page_lock);
  return;
}

void
page_remove (uint8_t *upage, struct thread *t)
{
  lock_acquire (&page_lock);

  struct page *p = page_find(upage, t);
  if (p != NULL)
    list_remove (&p->elem);

  lock_release (&page_lock);
  return;
}

struct page *
page_find (uint8_t *upage, struct thread *t)
{
  lock_acquire (&page_lock_2);

  struct list_elem *e;

  for (e = list_begin (&page_list); e != list_end (&page_list); e = list_next (e))
  {
    struct page *p = list_entry (e, struct page, elem);
    //printf("%x\n", p->upage);
    if ((p->upage == upage) && (p->thread == t))
    {
      lock_release(&page_lock_2);
      return p;
    }
  }
  lock_release(&page_lock_2);
  return NULL;
}

enum page_status
page_status (uint8_t *uaddr, struct thread *t)
{
  lock_acquire (&page_lock);
  struct page *p = page_find (uaddr, t);
  lock_release (&page_lock);

  if (p == NULL)
  {
    return PAGE_ERROR;
  }
  return p->status;
}

void
page_change_status (uint8_t *upage, struct thread *t, enum page_status status)
{
  struct page *p = page_find (upage, t);
  if (p == NULL)
  {
    // TODO: error handling
  }

  p->status = status;
}

bool
stack_growth (struct intr_frame *f UNUSED, uint8_t *upage, struct thread *t)
{
  bool stack_growth_cond = false;
  stack_growth_cond = ((unsigned)(f->esp) == upage + 4 || (unsigned)(f->esp) == upage + 32 || (unsigned)(f->esp) <= upage);

  uint8_t *upage_down = pg_round_down (upage);

  if(stack_growth_cond)
  {
    uint8_t *kpage = palloc_get_page_with_frame (PAL_USER | PAL_ZERO, upage_down, true);
    if (kpage != NULL) 
    {
      if (pagedir_get_page (t->pagedir, upage_down) == NULL
        && pagedir_set_page (t->pagedir, upage_down, kpage, true))
        return true;
      else
        palloc_free_page_with_frame (kpage);
    }
  }

  return false;
}











// ----------------------------------
static bool install_page (void *upage, void *kpage, bool writable);

void
page_stack_growth (void *fault_addr, void **esp)
{
	uint8_t *kpage;
	bool success = false;

	kpage = palloc_get_page (PAL_USER | PAL_ZERO);

  	if (kpage != NULL) 
    {
    	success = install_page ((uint8_t *) fault_addr, kpage, true);
    	if (success)
        	*esp = fault_addr;
      	else
        	palloc_free_page (kpage);
    }
	return;
}

static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}