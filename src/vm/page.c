#include "vm/page.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include <list.h>

static bool install_page (void *upage, void *kpage, bool writable);

struct page *
page_insert (void *upage, bool writable, enum page_status status)
{
  ASSERT (pg_ofs (upage) == 0);

  struct thread *t = thread_current();
  struct page *p = malloc(sizeof *p);

  switch (status)
  {
    case PAGE_FRAME:
      p->upage = upage;
      p->status = PAGE_FRAME;
      p->writable = writable;
      p->thread = thread_current();

      lock_acquire (&t->page_lock);
      hash_insert (&t->page_table, &p->elem);
      lock_release (&t->page_lock);

      ASSERT (page_find (t->page_table, upage) != NULL); // insert 잘 됐는지 확인 ㅇㅅaㅇ

      return p;

    case PAGE_FILE: /* for lazy loading */
      break;

    case PAGE_MMAP: /* for 3-2 mmap */
      break;

    default:
      break;
  }

  //printf("page insert null\n");
  return NULL;
}

void
page_remove (void *upage)
{
  // struct thread *t = thread_current();
  // hash_delete (&t->page_table, &p->elem);
  return;
}

struct page *
page_find (struct hash page_table, void *upage_input)
{
  uint8_t *upage = pg_round_down (upage_input);
  ASSERT (pg_ofs (upage) == 0);

  struct page p;
  struct hash_elem *e;

    p.upage = upage;
    e = hash_find (&page_table, &p.elem); // TODO: lock?

    if (e == NULL)
    {
      return NULL;
    }

    return hash_entry (e, struct page, elem);
}

bool
page_load (struct page *page)
{
  enum page_status status = page->status;
  //printf("enter page load\n");

  uint8_t *kpage;

  switch (status)
  {
    case PAGE_FRAME:
      break;
    case PAGE_SWAP:
      //printf("swap load\n");
      kpage = frame_alloc_with_page (PAL_USER, page);
      
      ASSERT (kpage != NULL);

        bool install = install_page (page->upage, kpage, page->writable);

        ASSERT (install != NULL);

      bool success = swap_in (kpage, page);

      if (!success)
      {
        return false;
      }

      page->status = PAGE_FRAME;
      return true;
    default:
      break;
  }

  return false;
}

bool
page_stack_growth (void *upage_input)
{
  uint8_t *upage = pg_round_down (upage_input);
  bool success = true;

    uint8_t *kpage = frame_alloc (PAL_USER | PAL_ZERO, upage, true);
    if (kpage != NULL)
    {
      success = install_page (upage, kpage, true);
      if (!success)
        frame_free (kpage);
    }
    return success;
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

























/* Hash table help funtions */

static unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED);
static bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);

static unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct page *p = hash_entry (p_, struct page, elem);
  return hash_bytes (&p->upage, sizeof p->upage); // TODO: 이거 맞는지 잘 모르겠다 확인 plz
}

static bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_,
                void *aux UNUSED)
{
  const struct page *a = hash_entry (a_, struct page, elem);
  const struct page *b = hash_entry (b_, struct page, elem);
  return a->upage < b->upage;
}

void
page_table_create (struct hash *page_table)
{
  hash_init (page_table, page_hash, page_less, NULL);
}

void
page_table_destroy (struct hash *page_table)
{
  // hash_destroy (struct hash *hash, hash action func *action);
  return;
}
