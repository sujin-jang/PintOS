#include "vm/swap.h"
#include "vm/page.h"
#include "threads/malloc.h"
#include "threads/synch.h"

/* PGSIZE / DISK_SECTOR_SIZE = 8 */
#define DISK_SECTOR_IN_FRAME 8
#define START_IDX 0

#define SLOT_ALLOCATE true
#define SLOT_FREE false

static struct swap * swap_find (uint8_t *upage, struct thread *t);

/* Swap Table */
struct list swap_list;
struct lock swap_lock;
struct lock disk_lock;
struct lock bitmap_lock;

struct disk *swap_disk;
struct lock entire_lock;
/* Bitmap that indicates allocate status of each slot: True = Allocate, False = Free */
/* TODO: 그냥 slot (=sector) 여덟개씩 묶어서 한 bit로 쳐도 괜찮을듯 */
struct bitmap *swap_slot;

struct hash *swap_table;

struct swap //너의 존재이유는 무엇이니
{
	uint8_t *upage; /* logical address: page */
	struct thread *thread;

	size_t index; /* index of swap slot */
	struct list_elem elem;
};

void
swap_init (void) 
{
	list_init (&swap_list);
	lock_init (&swap_lock);
	lock_init (&disk_lock);
	lock_init (&bitmap_lock);
	lock_init (&entire_lock);

	swap_disk = disk_get(1, 1);
	//disk_sector_t size = disk_size (swap_disk);
	printf("size: %x\n", disk_size(swap_disk));
	swap_slot = bitmap_create(disk_size(swap_disk));
}

/* allocate 된 swap slot list
   free 상태의 wsswap list */

void
swap_out (uint8_t *kpage, uint8_t *upage, struct thread *t)
{
	lock_acquire(&entire_lock);

	lock_acquire(&bitmap_lock);
	size_t idx = bitmap_scan_and_flip (swap_slot, 0, DISK_SECTOR_IN_FRAME, SLOT_FREE);
	lock_release(&bitmap_lock);

	//printf("user address: %x\n", upage);
	//printf("kernel address: %x\n", kpage);
	//printf("index: %x\n", idx);

	void * buffer;
	//hex_dump (kpage, buffer, 20, true);

	if (idx == BITMAP_ERROR) //TODO: error handling
	{
		lock_release(&entire_lock);
		printf("its full\n");
		return;
	}
	
	int i;
	// disk_sector_t sector_idx = idx * DISK_SECTOR_IN_FRAME;

	//printf("here\n");

	lock_acquire(&disk_lock);
	for (i = 0; i < DISK_SECTOR_IN_FRAME; i++)
		disk_write (swap_disk, idx + i, kpage + i * DISK_SECTOR_SIZE);
	lock_release(&disk_lock);
	
	//printf("swap out: %x\n", (unsigned)idx);

	/*
	struct swap *s = malloc (sizeof *s);
	s->index = idx;
	s->upage = upage;
	s->thread = t;

	lock_acquire(&swap_lock);
	list_push_back (&swap_list, &s->elem);
	lock_release(&swap_lock);
	*/
	
	struct page *p = page_find(upage, t);
	p->disk_index = idx;

	lock_release(&entire_lock);
	return;
}

/* free swap-slot: page is read back or the process whose page was swapped is terminated */
void
swap_in (uint8_t *upage, uint8_t *kpage, struct thread *t)
{
	lock_acquire(&entire_lock);

	//truct swap *s = swap_find (upage, t);
	//size_t idx = s->index;

	struct page *p = page_find(upage, t);
	size_t idx = p->disk_index;

	//printf("swap in: addr %x idx %x\n", upage, idx);

	lock_acquire(&bitmap_lock);
	bitmap_set_multiple(swap_slot, idx, DISK_SECTOR_IN_FRAME, 0);
	lock_release(&bitmap_lock);

	int i;
	//disk_sector_t sector_idx = idx * DISK_SECTOR_IN_FRAME;
	lock_acquire(&disk_lock);
	for (i = 0; i < DISK_SECTOR_IN_FRAME; i++)
		disk_read (swap_disk, idx + i, kpage + i * DISK_SECTOR_SIZE);
	lock_release(&disk_lock);

	/*
	lock_acquire(&swap_lock);
	list_remove(&s->elem);
	lock_release(&swap_lock);
	free (s);
	*/

	lock_release(&entire_lock);
	return;
}

static struct swap *
swap_find (uint8_t *upage, struct thread *t)
{
  struct list_elem *e;

  lock_acquire(&swap_lock);
  for (e = list_begin (&swap_list); e != list_end (&swap_list); e = list_next (e))
  {
    struct swap *s = list_entry (e, struct swap, elem);
    if ((s->upage == upage) && (s->thread == thread_current()))
    {
    	lock_release(&swap_lock);
    	return s;
    }
  }
  
  lock_release(&swap_lock);
  return NULL;
}

// TODO: 이해하렴
// You may use the BLOCK_SWAP block device for swapping, obtaining the struct block that represents it by calling block_get_role()

// 1 Sector = 512 byte
// 1 page = 4096 byte = 8 Sector

// TODO: malloc 처럼 2^n짜리 여러개 만들어놓고 넣뺐넣뺏 하면 더 빠르지않을까?
// TODO: sync -> lock



