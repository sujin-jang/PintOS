#include "vm/page.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "filesys/file.h"
#include <list.h>

struct mmap_mapping
{
  int id;
  struct file *file;
  void *addr;
  struct list_elem elem;
};

static bool install_page (void *upage, void *kpage, bool writable);

static int mmap_insert (struct file* file, void *addr);
static struct mmap_mapping * mmap_find (int mapid);

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

int
mmap_load (struct file *file, void *addr)
{
  // if addr is not page-aligned or if the range of pages mapped overlaps any existing set of mapped pages,
  // including the stack or pages mapped at executable load time.
  // It must also fail if addr is 0, because some Pintos code assumes virtual page 0 is not mapped

  uint8_t *upage = pg_round_down (addr);
  file_seek (file, 0);
  off_t read_bytes = file_length (file); 

  bool writable = true; // TODO: 이거 file의 writable 값을 가져와야되는데 함수가 없다 ㅠㅠ 만들까요

  int fail = -1;

  while (read_bytes > 0) 
    {
      /* Do calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = frame_alloc (PAL_USER, upage, writable);

      /* Choose a page to evict when no frames are free (by eviction policy) */
      if (kpage == NULL)
      {
        return fail;
      }

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          frame_free (kpage);
          return fail; 
        }

      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) 
        {
          frame_free (kpage);
          return fail;
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      upage += PGSIZE;
    }

  return mmap_insert(file, addr);
}


static int
mmap_insert (struct file* file, void *addr)
{
  struct thread *curr = thread_current ();
  struct mmap_mapping *mmap = malloc(sizeof (*mmap));

  //lock_acquire(&mmap_lock);
  curr->mmap_id++;
  mmap->id = curr->mmap_id;
  mmap->file = file;
  mmap->addr = addr;

  list_push_back (&curr->mmap_table, &mmap->elem);
  //lock_release(&mmap_lock);

  return curr->mmap_id;
}

static struct mmap_mapping *
mmap_find (int mapid)
{
  struct thread *curr = thread_current ();
  struct list_elem *iter;
  struct mmap_mapping *mmap;

  //lock_acquire(&mmap_lock);
  for (iter = list_begin (&curr->mmap_table); iter != list_end (&curr->mmap_table); iter = list_next (iter))
  {
    mmap = list_entry(iter, struct mmap_mapping, elem);
    if (mmap->id == mapid)
    {
      //lock_release(&mmap_lock);
      return mmap;
    }
  }
  //lock_release(&mmap_lock);
  return NULL;
}

void
mmap_unload (int mapid)
{
  // TODO: if dirty bit = ON 이면 write 해야하지만 귀찮으니까 그냥 write 한다 ^^

  // 1. file write
  struct mmap_mapping * mmap = mmap_find (mapid);
  ASSERT(mmap != NULL);

  struct thread *curr = thread_current();
  uint8_t *kpage;
  uint8_t *upage = pg_round_down (mmap->addr);
  struct file *file = mmap->file;

  file_seek (file, 0);

  //lock_acquire(&lock_file);
  off_t read_bytes = file_length (file);
  //lock_release(&lock_file);

  off_t ofs = 0; 

  while (read_bytes > 0) 
    {
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      kpage = pagedir_get_page (curr->pagedir, upage);
      //lock_acquire(&lock_file);
      if (pagedir_is_dirty (curr->pagedir, upage))
        ASSERT (file_write_at (file, kpage, page_read_bytes, ofs) == (int) page_read_bytes);
      //lock_release(&lock_file);

      pagedir_clear_page(curr->pagedir, upage); // TODO: page list에서 제거는?
      frame_free (kpage);

      read_bytes -= page_read_bytes;
      upage += PGSIZE;
      ofs += PGSIZE;
    }

  // 2. page / frame dealloc
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