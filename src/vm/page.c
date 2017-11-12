#include "vm/page.h"
#include "userprog/process.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include <stdio.h>

#include "userprog/syscall.h"

/* Supplemental Page Table
 * 1. page fault: kernel find out what data should be there
 * 2. process terminate: kernel decide what resources to free
 */

struct list page_list;
struct lock page_lock;
struct lock page_lock_2;
struct lock page_lock_sibal;

void
page_init (void) 
{
  list_init (&page_list);
  lock_init (&page_lock);
  lock_init (&page_lock_2);
  lock_init (&page_lock_sibal);
}

void
page_insert (uint8_t *upage, struct thread *t, bool writable)
{
  struct page *p = malloc (sizeof *p);
  p->upage = upage;
  p->thread = t;
  p->writable = writable;
  p->pin = false;

  lock_acquire (&page_lock);
  list_push_back (&page_list, &p->elem);
  lock_release (&page_lock);
  return;
}

void
page_remove (uint8_t *upage, struct thread *t)
{
  struct page *p = page_find(upage, t);
  if (p != NULL)
  {
    lock_acquire (&page_lock);
    list_remove (&p->elem);
    lock_release (&page_lock);
  }
  return;
}

struct page *
page_find (uint8_t *upage, struct thread *t)
{
  struct list_elem *e;

  lock_acquire (&page_lock_2);
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
page_status (uint8_t *upage, struct thread *t)
{
  uint8_t *upage_down = pg_round_down (upage);
  struct page *p = page_find (upage_down, t);

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
stack_growth (struct intr_frame *f UNUSED, uint8_t *upage, struct thread *t) // TODO: syscall 수정할ㄸㅐ 아래 스택 그로스랑 합치기
{
  bool stack_growth_cond = false;
  stack_growth_cond = ((unsigned)(f->esp) == upage + 4
    || (unsigned)(f->esp) == upage + 32
    || (unsigned)(f->esp) <= upage);

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
      {
        palloc_free_page_with_frame (kpage);
        return false;
      }
    }
  }
  return false;
}

bool
page_stack_growth (uint8_t *upage, struct thread *t)
{
  uint8_t *kpage = palloc_get_page_with_frame (PAL_USER | PAL_ZERO, upage, true);
  if (kpage != NULL)
  {
    if (pagedir_get_page (t->pagedir, upage) == NULL
      && pagedir_set_page (t->pagedir, upage, kpage, true))
        return true;
      else
        palloc_free_page_with_frame (kpage);
    }
  return false;
}

void
page_set_file (struct file *file, uint8_t *upage, off_t ofs, uint32_t page_read_bytes, uint32_t page_zero_bytes, bool writable)
{
  struct load_info *load_info = malloc(sizeof *load_info); // TODO: free: lazy load할 때
  load_info -> file = file;
  load_info -> ofs = ofs;
  load_info -> read_bytes = page_read_bytes;
  load_info -> zero_bytes = page_zero_bytes;

  struct page *p = malloc(sizeof *p);
  p->upage = upage;
  p->thread = thread_current();
  p->status = PAGE_FILE;

  p->load_info = load_info;
  p->pin = true;
  p->load = false;
  p->writable = writable;

  lock_acquire (&page_lock);
  list_push_back (&page_list, &p->elem);
  lock_release (&page_lock);
}

bool
page_load_file (uint8_t *upage)
{
  struct thread *t = thread_current();
  struct page *p = page_find (upage, t);
  
  uint8_t *kpage = palloc_get_page_with_frame_modify (PAL_USER, p); //TODO: writable = page의 writable상태로
  pagedir_set_page (t->pagedir, upage, kpage, p->writable);

  struct load_info *info = p->load_info;

  lock_acquire(&lock_file);
  if (file_read_at (info->file, kpage, info->read_bytes, info->ofs) != (int) info->read_bytes)
    {
      lock_release(&lock_file);

      palloc_free_page_with_frame (kpage);
      pagedir_clear_page (t->pagedir, upage);

      return false;
    }
  lock_release(&lock_file);
  memset (kpage + info->read_bytes, 0, info->zero_bytes);

  // p->status = PAGE_FRAME;
  p->pin = false;
  p->load = true;

  return true;
}
