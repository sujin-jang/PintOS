#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "vm/frame.h"

//Initialize frame
void frame_init(){
  list_init(&frame_list);
  lock_init(&lock_frame);
}

//Allocate page size memory to frame
void* frame_alloc(enum palloc_flags flag)
{
  if ( (flags & PAL_USER) == 0 ) // have to be user
    return NULL;

  void *frame = palloc_get_page(flag);

  if (frame == NULL)
    frame_add_table(frame);
  else{
    if (!frame_evict(frame))
      PANIC ("Frame can't be evicted due to insufficient swap space");
  }
  return frame;
}

// Release the frame
void frame_free(void *frame){
  struct list_elem *e;
  
  lock_acquire(&lock_frame);
  for (e = list_begin(&frame_list); e != list_end(&frame_list);
      e = list_next(e)){
    struct frame_ent *frame_e = list_entry(e, struct frame_ent, elem);
    if (frame_e->frame == frame){
      list_remove(e);
      free(frame_e);
      break;
    }
  }
  lock_release(&lock_frame);
  palloc_free_page(frame);
}


// Add frame to the frame table
void frame_add_table(void *frame){
  struct frame_ent *frame_e = malloc(sizeof(struct frame_ent));
  frame_e->frame = frame;
  frame_e->thread = thread_current();

  lock_acquire(&lock_frame);
  list_push_back(&frame_list, &frame_e->elem);
  lock_release(&lock_frame);
}

// Check whether the frame have to be evict or not
bool frame_evict (void *frame){
  return false;
}
