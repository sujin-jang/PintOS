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

static int get_user (const uint8_t *uaddr);

static void is_valid_ptr (struct intr_frame *f UNUSED, void *uaddr);
static void is_valid_buffer (struct intr_frame *f UNUSED, void *uaddr, unsigned length, bool write);
static void is_valid_page (struct intr_frame *f UNUSED, void *uaddr, bool write);

static int file_add_fdlist (struct file* file);
static void file_remove_fdlist (int fd);
static struct file_descriptor * fd_to_file_descriptor (int fd);

struct lock lock_file;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&lock_file);
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

      is_valid_buffer(f, *(void **)arg2, (unsigned)*arg3, true);
      
      if((int)*arg1 == 0)
      {
        //printf("syscall exit here1\n");
        syscall_exit(EXIT_STATUS_1);
      }

      f->eax = (uint32_t) syscall_read((int)*arg1, *(void **)arg2, (unsigned)*arg3);
      break;
    case SYS_WRITE: //9

      is_valid_buffer(f, *(void **)arg2, (unsigned)*arg3, false);
      
      if((int)*arg1 == 0)
      {
        //printf("syscall exit here2\n");
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
      break;
    case SYS_MUNMAP:
      // Not implemented yet
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
    return -1;

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
