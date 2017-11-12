#include "vm/frame.h"
#include "vm/swap.h"
#include "vm/page.h"

#include "threads/vaddr.h"
#include "threads/synch.h"

/* Frame Table */
struct list frame_list;
struct lock frame_lock;
struct lock frame_lock_2;

static struct frame * frame_find (uint8_t *kpage);

struct frame
{
  uint8_t *kpage; /* physical address: frame */
  uint8_t *upage; /* logical address: page */
  struct thread *thread;

  struct list_elem elem;
};

void
frame_init (void) 
{
  list_init (&frame_list);
  lock_init (&frame_lock);
  lock_init (&frame_lock_2);
}

void
frame_insert (uint8_t *kpage, uint8_t *upage, struct thread *t)
{
  struct frame *f = malloc(sizeof *f);

  f->kpage = kpage;
  f->upage = upage;
  f->thread = t;

  lock_acquire(&frame_lock);
  list_push_back (&frame_list, &f->elem);
  lock_release(&frame_lock);
  
}

/* access 할때마다 list 내에서 순서를 바꿔주면 좋을텐데 */
/* TODO:
  eviction 해서는 안되는 애들 표시
  LRU
  */

/* No frames are free, choose a frame to evict */
void
frame_eviction (void)
{
  struct list_elem *elem = list_pop_back (&frame_list); //TODO: modify to LRU

  struct frame *f = list_entry (elem, struct frame, elem); // frame to
  palloc_free_page (f->kpage);
  pagedir_clear_page (thread_current()->pagedir, f->upage); 

  swap_out (f->kpage, f->upage, f->thread);
  page_change_status (f->upage, f->thread, PAGE_SWAP);
}

// TODO: tests/Make.tests line 17 TIMEOUT = 10 -> 60

void *
palloc_get_page_with_frame (enum palloc_flags flags, uint8_t *upage, bool writable) 
{
  //return palloc_get_page (flags);

  //printf("palloc upage: %x\n", upage);
  uint8_t *kpage;
  struct thread *t = thread_current();

  if ( (flags & PAL_USER) == 0 || is_kernel_vaddr(upage) )
    return NULL;

  // TODO: try exception으로 예쁘게 적어보렴, lock 걸어놓자, PAL_USER 아닌 경우 거르기 -> PAL_USER | ZERO 이런애들은?

  /* No frames are free, choose a frame to evict */
  if ((kpage = palloc_get_page (flags)) == NULL)
  {
    frame_eviction(); // TODO: 얘를 bool return 시켜서 evict 불가능한 케이스를 알려주자 -> kernel panic
    kpage = palloc_get_page (flags);
  }

  /* Free frame exists */
  if (kpage != NULL){
    frame_insert(kpage, upage, t);
    //printf("kpage: %x, upage: %x\n", kpage, upage);
    page_insert (upage, t, writable);
  }

  //printf("palloc get page: %x %x\n", kpage, upage);
  return kpage;
}

void
palloc_free_page_with_frame (uint8_t *kpage)
{
  /* remove from frame list */
  struct frame *f = frame_find(kpage);
  if (f != NULL)
  {
    lock_acquire(&frame_lock);
    list_remove (&f->elem);
    lock_release(&frame_lock);
    
    page_remove (f->upage, f->thread);
  }

  palloc_free_page (kpage);
  return;
}

static struct frame *
frame_find (uint8_t *kpage)
{
  struct list_elem *e;

  for (e = list_begin (&frame_list); e != list_end (&frame_list); e = list_next (e))
  {
    struct frame *f = list_entry (e, struct frame, elem);
    if ((f->kpage == kpage) && (f->thread == thread_current()))
      return f;
  }

  return NULL;
}
