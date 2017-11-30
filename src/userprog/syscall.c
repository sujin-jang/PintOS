#include "userprog/syscall.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "threads/malloc.h"

#include "filesys/filesys.h"
#include "filesys/file.h"

#include "vm/page.h"

#include <string.h>
#include <stdlib.h>

#define EXIT_STATUS_1 -1
typedef int mapid_t;
 
static void syscall_handler (struct intr_frame *);
//static void syscall_exit (int status);
static bool syscall_create (char *file, off_t initial_size);
static int syscall_open (char *file);
static void syscall_close (int fd);
static int syscall_read (int fd, void *buffer, unsigned length);
static int syscall_filesize (int fd);
static int syscall_write (int fd, void *buffer, unsigned size);
static bool syscall_remove (const char *file);
static bool syscall_seem (const char *file);
static void syscall_seek (int fd, unsigned position);
static unsigned syscall_tell (int fd);

static mapid_t syscall_mmap (int fd, void *addr);
static void syscall_munmap (mapid_t mapid);

static int get_user (const uint8_t *uaddr);

static void is_valid_ptr (struct intr_frame *f UNUSED, void *uaddr);
static void is_valid_buffer (struct intr_frame *f UNUSED, void *uaddr, unsigned length, bool write);
static void is_valid_page (struct intr_frame *f UNUSED, void *uaddr, bool write);

static int file_add_fdlist (struct file* file);
static void file_remove_fdlist (int fd);
static struct file_descriptor * fd_to_file_descriptor (int fd);
static void process_remove_mmaptable (struct thread* t);

static bool install_page (void *upage, void *kpage, bool writable);
static int mmap_load (struct file *file, void *addr);
static int mmap_insert (struct file* file, void *addr);
static struct mmap_mapping * mmap_find (int mapid);
static void mmap_unload (int mapid);

struct lock lock_file;
struct lock mmap_lock;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&lock_file);
  lock_init (&mmap_lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  //struct thread* curr = thread_current();
  int *syscall_nr = (int *)(f->esp);
  int i;

  //Check whether stack pointer exceed PHYS_BASE
  for(i = 0; i < 4; i++){ //Argument of syscall is no more than 3
    if( ( get_user((uint8_t *)syscall_nr + 4 * i ) == -1 )
        || is_kernel_vaddr ((void *)((uint8_t *)syscall_nr + 4 * i)) )
    {
      //printf("syscall exit get user\n");
      syscall_exit(EXIT_STATUS_1);
    }
  }

  void **arg1 = (void **)(syscall_nr+1);
  void **arg2 = (void **)(syscall_nr+2);
  void **arg3 = (void **)(syscall_nr+3);
  //void **arg4 = (void **)(syscall_nr+4);

  switch (*syscall_nr) {
    case SYS_HALT: //0
      power_off();
      break;
    case SYS_EXIT: //1
      syscall_exit((int)*arg1);
      break;
    case SYS_EXEC: //2
      is_valid_page(f, *(void **)arg1, false);
      f->eax = (uint32_t) process_execute(*(char **)arg1);
      break;
    case SYS_WAIT: //3
      f->eax = (uint32_t) process_wait(*(tid_t *)arg1);
      break; 
    case SYS_CREATE: //4
      is_valid_page(f, *(void **)arg1, false);
      f->eax = (uint32_t) syscall_create(*(char **)arg1, (off_t)*arg2);
      break;
    case SYS_REMOVE: //5
      f->eax = (uint32_t) syscall_remove (*(char **)arg1);
      break;
    case SYS_OPEN: //6
      is_valid_page(f, *(void **)arg1, false);
      f->eax = (uint32_t) syscall_open(*(char **)arg1);
      break;
    case SYS_FILESIZE: //7
      f->eax = (uint32_t) syscall_filesize((int)*arg1);
      break;
    case SYS_READ: //8
      //printf("syscall read\n");
      is_valid_buffer(f, *(void **)arg2, (unsigned)*arg3, true);
      
      if((int)*arg1 == 0)
      {
        //printf("syscall exit here1\n");
        syscall_exit(EXIT_STATUS_1);
      }

      f->eax = (uint32_t) syscall_read((int)*arg1, *(void **)arg2, (unsigned)*arg3);
      break;
    case SYS_WRITE: //9
      //printf("syscall write\n");
      is_valid_buffer(f, *(void **)arg2, (unsigned)*arg3, false);
      if((int)*arg1 == 0)
      {
        //printf("syscall write exit 1\n");
        syscall_exit(EXIT_STATUS_1); //STDIN은 exit/ todo: valid ptr 안에 집어넣기
      }

      f->eax = (uint32_t) syscall_write ((int)*arg1, *(void **)arg2, (unsigned)*arg3);
      break;
    case SYS_SEEK:
      //printf("syscall seek\n");
      syscall_seek ((int)*arg1, (unsigned)*arg2);
      break;
    case SYS_TELL:
      printf("syscall tell\n");
      // Not implemented
      break;
    case SYS_CLOSE:
      syscall_close((int)*arg1);
      break;
/* ---------------------------------------------------------------------*/
/* 
 * Haney: No need to implement below since we are working on project 2
 */
    case SYS_MMAP:
      // Not implemented yet
      // is_valid_page(f, *(void **)arg2, false); /* TODO: is_valid_buffer로 file length만큼 validity check */
      f->eax = (uint32_t) syscall_mmap ((int)*arg1, *(void **)arg2);
      break;
    case SYS_MUNMAP:
      // Not implemented yet
      syscall_munmap ((mapid_t)*arg1);
      break;
    case SYS_CHDIR:
      // Not implemented yet
      break;
    case SYS_MKDIR:
      // Not implemented yet
      break;
    case SYS_READDIR: 
      // Not implemented yet
      break;
    case SYS_ISDIR:
      // Not implemented yet
      break;
    case SYS_INUMBER:
      // Not implemented yet
      break;
/* ---------------------------------------------------------------------*/
    default:
      break;
  }
}

static int get_user (const uint8_t *uaddr){
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
      : "=&a" (result) : "m" (*uaddr));
  return result;
}

/* Handle invalid memory access:
   exit(-1) when UADDR is kernel vaddr or unmapped to pagedir of current process */

static void
is_valid_ptr (struct intr_frame *f UNUSED, void *uaddr)
{
  if (is_kernel_vaddr(uaddr))
  {
    //printf("syscall exit4\n");
    syscall_exit(EXIT_STATUS_1);
  }

  struct thread* t = thread_current ();
  uint32_t *pd = t->pagedir;
  uint32_t *is_bad_ptr = pagedir_get_page (pd, uaddr);
  
  if(is_bad_ptr == NULL)
  {
    //printf("syscall exit3\n");
    syscall_exit(EXIT_STATUS_1);
  }
}

static void
is_valid_page (struct intr_frame *f UNUSED, void *uaddr, bool write)
{
  if (is_kernel_vaddr(uaddr))
  {
    //printf("syscall exit5\n");
    syscall_exit(EXIT_STATUS_1);
  }

  struct thread *t = thread_current ();
  uint32_t *pd = t->pagedir;

  struct page *p = page_find (t->page_table, uaddr);
  bool writable;

  if (p != NULL)
  {
    writable = p->writable;

    if ((write == true) && (writable == false))
    {
      //printf("syscall exit2\n");
      syscall_exit(EXIT_STATUS_1);
    }
  }
  
  if(pagedir_get_page (pd, uaddr) == NULL)
  {
   //printf ("this is null page\n");

    if (p != NULL)
    {
      //printf("Load page\n");
      page_load (p);
    }

    bool stack_growth_cond = (f->esp <= uaddr + 32); // && write;
    if (stack_growth_cond)
    {
      
      if (page_stack_growth (uaddr))
        return;
    }
    //printf("syscall exit1\n");
    syscall_exit(-1);
  }
  return;
}

static void
is_valid_buffer (struct intr_frame *f UNUSED, void *uaddr, unsigned length, bool write)
{
  void *position = uaddr;
  
  while (pg_round_down(position) <= pg_round_down(uaddr + length))
  {
    is_valid_page (f, position, write);
    //position += 1;
    position += PGSIZE;
  }
  return;
}

/************************************************************
*      struct and function for file descriptor table.       *
*************************************************************/

struct file_descriptor
{
  int fd;
  struct file* file;
  struct list_elem elem;
};

static int
file_add_fdlist (struct file* file)
{
  struct thread *curr = thread_current ();
  struct file_descriptor *desc = malloc(sizeof (*desc));

  curr->fd++;

  desc->fd = curr->fd;
  desc->file = file;

  list_push_back (&curr->fd_list, &desc->elem);

  return curr->fd;
}

static void
file_remove_fdlist (int fd)
{
  struct file_descriptor *desc = fd_to_file_descriptor(fd);

  if (desc == NULL)
    return;

  file_close (desc->file);
  list_remove (&desc->elem);
  free (desc);
}

static struct file_descriptor *
fd_to_file_descriptor (int fd)
{
  struct thread *curr = thread_current ();
  struct list_elem *iter;
  struct file_descriptor *desc;

  for (iter = list_begin (&curr->fd_list); iter != list_end (&curr->fd_list); iter = list_next (iter))
  {
    desc = list_entry(iter, struct file_descriptor, elem);
    if (desc->fd == fd)
      return desc;
  }
  return NULL;
}

static void
process_remove_fdlist (struct thread* t)
{
  struct list_elem *iter, *iter_2;
  struct file_descriptor *desc;

  if(list_empty(&t->fd_list))
    return;

  while((iter = list_begin (&t->fd_list)) != list_end (&t->fd_list))
  {
    iter_2 = list_next (iter);

    desc = list_entry(iter, struct file_descriptor, elem);

    file_close (desc->file);
    list_remove (&desc->elem);
    free (desc);

    iter = iter_2;
  }
}

static void
process_remove_mmaptable (struct thread* t)
{
  struct list_elem *iter, *iter_2;
  struct mmap_mapping *mmap;

  if(list_empty(&t->mmap_table))
  {
    return;
  }

  while((iter = list_begin (&t->mmap_table)) != list_end (&t->mmap_table))
  {
    iter_2 = list_next (iter);
    mmap = list_entry(iter, struct mmap_mapping, elem);

    mmap_unload (mmap->id);
    iter = iter_2;
  }
}

/************************************************************
*              system call helper function.                 *
*************************************************************/

void
syscall_exit (int status)
{
  struct thread *curr = thread_current();
  struct thread *parent = curr->parent;
  struct list_elem* e;
  struct child *child_process;
 
  for(e = list_begin(&parent->child);
      e != list_end (&parent->child);
      e = list_next (e))
  {
    child_process = list_entry(e, struct child, elem);
    if (child_process->tid == curr->tid){
      child_process->exit_stat = status;

      process_remove_mmaptable(curr);
      process_remove_fdlist(curr);

      char* save_ptr;
      strtok_r (&curr->name," ", &save_ptr);

      printf("%s: exit(%d)\n", curr->name, status);
      sema_up(curr->process_sema);
      thread_exit();
      NOT_REACHED();
    }
  }
  NOT_REACHED();
}

static bool
syscall_create (char *file, off_t initial_size)
{
  return filesys_create (file, initial_size);
}

static int
syscall_open (char *file)
{
  lock_acquire(&lock_file);
  struct file* opened_file = filesys_open (file);

  if (opened_file == NULL)
  {
    lock_release(&lock_file);
    return -1;
  }

  int result = file_add_fdlist(opened_file);
  lock_release(&lock_file);
  return result;
}

static void
syscall_close (int fd)
{
  lock_acquire(&lock_file);
  file_remove_fdlist (fd);
  lock_release(&lock_file);
}

static int
syscall_read (int fd, void *buffer, unsigned length)
{
  if (fd == 0) /* STDIN */
    return input_getc();

  lock_acquire(&lock_file);

  struct file_descriptor *desc = fd_to_file_descriptor(fd);

  if (desc == NULL)
  {
    lock_release(&lock_file);
    return -1;
  }

  int result = file_read (desc->file, buffer, length);

  lock_release(&lock_file);
  return result;
}

static int
syscall_filesize (int fd)
{
  lock_acquire(&lock_file);
  struct file_descriptor *desc = fd_to_file_descriptor(fd);
  int result = file_length(desc->file);

  lock_release(&lock_file);
  return result;
}

static int
syscall_write (int fd, void *buffer, unsigned size)
{
  //printf("input size: %d, fd: %d\n", size, fd);
  if (fd == 1) /* STDOUT */
  {
    putbuf (buffer, size);
    return size;
  }

  lock_acquire(&lock_file);

  struct file_descriptor *desc = fd_to_file_descriptor(fd);

  if (desc == NULL)
  {
    lock_release(&lock_file);
    return -1;
  }

  int result = file_write (desc->file, buffer, size);

  lock_release(&lock_file);

  //printf("return size: %d\n", result);
  return result;
}

static bool
syscall_remove (const char *file)
{
  lock_acquire(&lock_file);
  bool result = filesys_remove (file);
  lock_release(&lock_file);

  return result;
}

static void
syscall_seek (int fd, unsigned position)
{
  lock_acquire(&lock_file);

  struct file_descriptor *desc = fd_to_file_descriptor(fd);

  if (desc == NULL)
  {
    lock_release(&lock_file);
  }

  file_seek (desc->file, position);
  lock_release(&lock_file);
}

static unsigned
syscall_tell (int fd)
{
  lock_acquire(&lock_file);

  struct file_descriptor *desc = fd_to_file_descriptor(fd);

  if (desc == NULL)
  {
    lock_release(&lock_file);
    return 0; //todo: error handling right?
  }

  unsigned result = file_tell (desc->file);
  lock_release(&lock_file);

  return result;
}

static mapid_t
syscall_mmap (int fd, void *addr)
{
  if ((addr == NULL) || ((uint8_t)addr % PGSIZE != 0))
    return -1;

  struct file_descriptor *desc = fd_to_file_descriptor (fd);
  if (desc == NULL)
    return -1;

  lock_acquire(&lock_file);
  struct file *f = file_reopen(desc->file);
  lock_release(&lock_file);

  mapid_t id = mmap_load (f, addr);

  // printf("mapid: %d\n", id);

  return id;
}

static void
syscall_munmap (mapid_t mapid)
{
  mmap_unload (mapid);
  return;
}

// ----------

static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

static int
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

  file_seek (file, 0);
  return mmap_insert(file, addr);
}


static int
mmap_insert (struct file* file, void *addr)
{
  struct thread *curr = thread_current ();
  struct mmap_mapping *mmap = malloc(sizeof (*mmap));

  lock_acquire(&mmap_lock);
  curr->mmap_id++;
  mmap->id = curr->mmap_id;
  mmap->file = file;
  mmap->addr = addr;

  list_push_back (&curr->mmap_table, &mmap->elem);
  lock_release(&mmap_lock);

  return curr->mmap_id;
}

static struct mmap_mapping *
mmap_find (int mapid)
{
  struct thread *curr = thread_current ();
  struct list_elem *iter;
  struct mmap_mapping *mmap;

  lock_acquire(&mmap_lock);
  for (iter = list_begin (&curr->mmap_table); iter != list_end (&curr->mmap_table); iter = list_next (iter))
  {
    mmap = list_entry(iter, struct mmap_mapping, elem);
    if (mmap->id == mapid)
    {
      lock_release(&mmap_lock);
      return mmap;
    }
  }
  lock_release(&mmap_lock);
  return NULL;
}

static void
mmap_unload (int mapid)
{
  struct mmap_mapping * mmap = mmap_find (mapid);
  ASSERT(mmap != NULL);

  struct thread *curr = thread_current();
  uint8_t *kpage;
  uint8_t *upage = pg_round_down (mmap->addr);
  struct file *file = mmap->file;

  file_seek (file, 0);

  lock_acquire(&lock_file);
  off_t read_bytes = file_length (file);
  lock_release(&lock_file);

  off_t ofs = 0; 

  while (read_bytes > 0) 
    {
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      kpage = pagedir_get_page (curr->pagedir, upage);

      lock_acquire(&lock_file);
      if (pagedir_is_dirty (curr->pagedir, upage))
      {
        ASSERT (file_write_at (file, kpage, page_read_bytes, ofs) == (int) page_read_bytes);
      }
      lock_release(&lock_file);

      pagedir_clear_page(curr->pagedir, upage); // TODO: page list에서 제거는?
      frame_free (kpage);

      read_bytes -= page_read_bytes;
      upage += PGSIZE;
      ofs += PGSIZE;
    }

  lock_acquire(&lock_file);
  file_close(mmap->file);
  lock_release(&lock_file);

  list_remove (&mmap->elem);
  free (mmap);
}
