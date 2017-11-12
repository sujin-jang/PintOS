#include "vm/swap.h"
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

struct disk *swap_disk;

/* Bitmap that indicates allocate status of each slot: True = Allocate, False = Free */
/* TODO: 그냥 slot (=sector) 여덟개씩 묶어서 한 bit로 쳐도 괜찮을듯 */
struct bitmap *swap_slot;

struct hash *swap_table;

struct swap
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

	swap_disk = disk_get(1, 1);
	disk_sector_t size = disk_size (swap_disk);
	swap_slot = bitmap_create (size / DISK_SECTOR_IN_FRAME);
}

/* allocate 된 swap slot list
   free 상태의 wsswap list */

void
swap_out (uint8_t *kpage, uint8_t *upage, struct thread *t)
{
	lock_acquire(&swap_lock);
	size_t idx = bitmap_scan_and_flip (swap_slot, START_IDX, 1, SLOT_FREE);

	if (idx == BITMAP_ERROR) //TODO: error handling
	{
		lock_release(&swap_lock);
		return;
	}
	
	int i;
	// disk_sector_t sector_idx = idx * DISK_SECTOR_IN_FRAME;

	for (i = 0; i < DISK_SECTOR_IN_FRAME; i++)
		disk_write (swap_disk, idx * DISK_SECTOR_IN_FRAME + i, kpage + i * DISK_SECTOR_SIZE);
	
	printf("swap out: %x\n", (unsigned)idx);

	struct swap *s = malloc (sizeof *s);
	s->index = idx;
	s->upage = upage;
	s->thread = t;

	list_push_back (&swap_list, &s->elem);
	lock_release(&swap_lock);
	return;
}

/* free swap-slot: page is read back or the process whose page was swapped is terminated */
void
swap_in (uint8_t *upage, uint8_t *kpage, struct thread *t)
{
	lock_acquire(&swap_lock);

	struct swap *s = swap_find (upage, t);
	size_t idx = s->index;

	printf("swap in: %x\n", idx);

	if (bitmap_test (swap_slot, idx) == SLOT_ALLOCATE)
		bitmap_flip (swap_slot, idx);

	int i;
	//disk_sector_t sector_idx = idx * DISK_SECTOR_IN_FRAME;
	for (i = 0; i < DISK_SECTOR_IN_FRAME; i++)
	{
		disk_read (swap_disk, idx * DISK_SECTOR_IN_FRAME + i, kpage + i * DISK_SECTOR_SIZE);
	}

	list_remove(&s->elem);
	free (s);
	lock_release(&swap_lock);
	return;
}

static struct swap *
swap_find (uint8_t *upage, struct thread *t)
{
  struct list_elem *e;

  for (e = list_begin (&swap_list); e != list_end (&swap_list); e = list_next (e))
  {
    struct swap *s = list_entry (e, struct swap, elem);
    if (s->upage == upage)
      return s;
  }

  return NULL;
}

// TODO: 이해하렴
// You may use the BLOCK_SWAP block device for swapping, obtaining the struct block that represents it by calling block_get_role()

// 1 Sector = 512 byte
// 1 page = 4096 byte = 8 Sector

// TODO: malloc 처럼 2^n짜리 여러개 만들어놓고 넣뺐넣뺏 하면 더 빠르지않을까?
// TODO: sync -> lock



