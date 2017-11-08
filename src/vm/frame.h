#ifndef FRAME_H
#define FRAME_H

#include <stdint.h>
#include <stdbool.h>
#include <list.h>

struct list frame_list;
struct lock lock_frame;

struct frame_ent{
  struct thread *thread;  //Thread which hold current frame
  void *frame;            //Address of frame
  struct list_elem elem;
};

void frame_init(void);
void* frame_alloc(enum palloc_flags flag);
void frame_free(void *frame);
void frame_add_table(void *frame);
bool frame_evict(void *frame);

#endif
