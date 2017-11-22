#include "vm/swap.h"
#include <bitmap.h>
#include "devices/disk.h"

#define DISK_SECTOR_IN_FRAME 8
#define START_IDX 0

#define SLOT_ALLOCATE true
#define SLOT_FREE false

struct bitmap *swap_table;
struct disk *swap_disk;

struct lock swap_lock;
struct lock disk_lock;

void
swap_init (void) 
{
	lock_init (&swap_lock);
	lock_init (&disk_lock);

	swap_disk = disk_get(1, 1);
	swap_table = bitmap_create(disk_size(swap_disk));
}

void
swap_out (struct frame *frame)
{
	lock_acquire(&swap_lock);
	size_t idx = bitmap_scan_and_flip (swap_table, 0, DISK_SECTOR_IN_FRAME, SLOT_FREE);
	lock_release(&swap_lock);

	if (idx == BITMAP_ERROR) //TODO: error handling KERNEL
	{
		return;
	}
	
	int i;
	uint8_t *kpage = frame->kpage;
	struct page *p = frame->page;

	lock_acquire(&disk_lock);
	for (i = 0; i < DISK_SECTOR_IN_FRAME; i++)
		disk_write (swap_disk, idx + i, kpage + i * DISK_SECTOR_SIZE);
	lock_release(&disk_lock);
	
	//printf("swap out: %x\n", (unsigned)idx);

	p->swap_index = idx; //TODO: page lock // TODO: frame_table에서 지우
	return;
}

bool
swap_in (void *kpage, struct page *p)
{
	size_t idx = p->swap_index;

	//printf("swap in: addr %x idx %x\n", upage, idx);

	lock_acquire(&swap_lock);
	bitmap_set_multiple(swap_table, idx, DISK_SECTOR_IN_FRAME, 0);
	lock_release(&swap_lock);

	int i;

	lock_acquire(&disk_lock);
	for (i = 0; i < DISK_SECTOR_IN_FRAME; i++)
		disk_read (swap_disk, idx + i, kpage + i * DISK_SECTOR_SIZE);
	lock_release(&disk_lock);

	return true;
}

// process terminate: 특정 process의 swap들 free시키기

void
swap_free (void)
{

}
