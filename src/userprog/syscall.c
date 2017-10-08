#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

#include "filesys/filesys.h"
#include "filesys/file.h"

#include <string.h>
#include <stdlib.h>
 
static void syscall_handler (struct intr_frame *);
static void syscall_exit (struct intr_frame *f UNUSED, bool status);
static bool syscall_create (struct intr_frame *f UNUSED, char *file, off_t initial_size);
static int syscall_open (struct intr_frame *f UNUSED, char *file);
static void syscall_close (struct intr_frame *f UNUSED, int fd);
static int syscall_read (struct intr_frame *f UNUSED, int fd, void *buffer, unsigned length);
static int syscall_filesize (struct intr_frame *f UNUSED, int fd);
static int syscall_write (struct intr_frame *f UNUSED, int fd, void *buffer, unsigned size);

static int get_user (const uint8_t *uaddr);

static int file_add_fdlist (struct file* file);
static void file_remove_fdlist (int fd);
static struct file_descriptor * fd_to_file_descriptor (int fd);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  //struct thread* curr = thread_current();
  int *syscall_nr = (int *)(f->esp);
  int i;
  int res;

  //Check whether stack pointer exceed PHYS_BASE
  for(i = 0; i < 4; i++){ //Argument of syscall is no more than 3
    if( ( get_user((uint8_t *)syscall_nr + 4 * i ) == -1 )
        || ((void *)((uint8_t *)syscall_nr + 4 * i) >= PHYS_BASE) ){
      syscall_exit(f, false);
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
      syscall_exit(f, true);
  		break;
    case SYS_EXEC: //2
      f->eax = (uint32_t) process_execute(*(char **)arg1);
      break;
  	case SYS_WAIT: //3
      f->eax = (uint32_t) process_wait(*(tid_t *)arg1);
  		break; 
    case SYS_CREATE: //4
      f->eax = (uint32_t) syscall_create(f, *(char **)arg1, (off_t)*arg2);
      break;
    case SYS_REMOVE: //5
      // Not implemented
      break;
    case SYS_OPEN: //6
      f->eax = (uint32_t) syscall_open(f, *(char **)arg1);
      break;
    case SYS_FILESIZE: //7
      f->eax = (uint32_t) syscall_filesize(f, (int)*arg1);
      break;
    case SYS_READ: //8
      f->eax = (uint32_t) syscall_read(f, (int)*arg1, *(void **)arg2, (unsigned)*arg3);
      break;
  	case SYS_WRITE: //9
  		f->eax = (uint32_t) syscall_write (f, (int)*arg1, *(void **)arg2, (unsigned)*arg3);
  		break;
    case SYS_SEEK:
      // Not implemented
      break;
    case SYS_TELL:
      // Not implemented
      break;
    case SYS_CLOSE:
      syscall_close(f, (int)*arg1);
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
  struct file_descriptor *desc = malloc(sizeof *desc);
  memset (desc, 0, sizeof *desc);

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
  // TODO: free memory 
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
    {
      return desc;
    }
  }
  return NULL;
}

/************************************************************
*              system call helper function.                 *
*************************************************************/

static void
syscall_exit (struct intr_frame *f UNUSED, bool flag)
{
  int exit_stat;
  if(flag == false)
    exit_stat = -1;
  else
    exit_stat = *((int *)(f->esp)+1);
  struct thread *curr = thread_current();
  struct thread *parent = curr->parent;
  struct list_elem* e;
  struct child *child_process;
  for(e = list_begin(&parent->child);
      e != list_end (&parent->child);
      e = list_next (e)){
    child_process = list_entry(e, struct child, elem);
    if (child_process->tid == curr->tid)
      child_process->exit_stat = exit_stat;
  }
  printf("%s: exit(%d)\n", curr->name, exit_stat);
  lock_release(curr->process_lock);
  thread_exit();

  // TODO: Close all fd in fd list
}

static bool
syscall_create (struct intr_frame *f UNUSED, char *file, off_t initial_size)
{
  if (file == NULL)
    syscall_exit(f, false);

  return filesys_create (file, initial_size);
}

static int
syscall_open (struct intr_frame *f UNUSED, char *file)
{
  if (file == NULL)
    syscall_exit(f, false);

  struct file* opened_file = filesys_open (file);

  if (opened_file == NULL)
    return -1;

  return file_add_fdlist(opened_file);
}

static void
syscall_close (struct intr_frame *f UNUSED, int fd)
{
  file_remove_fdlist (fd);
}

static int
syscall_read (struct intr_frame *f UNUSED, int fd, void *buffer, unsigned length)
{
  if (fd == 0) /* STDIN */
    return input_getc();

  struct file_descriptor *desc = fd_to_file_descriptor(fd);

  if (desc == NULL)
    return -1;

  return file_read (desc->file, buffer, length);
}

static int
syscall_filesize (struct intr_frame *f UNUSED, int fd)
{
  struct file_descriptor *desc = fd_to_file_descriptor(fd);
  return file_length(desc->file);
}

static int
syscall_write (struct intr_frame *f UNUSED, int fd, void *buffer, unsigned size)
{
  if (fd == 1) /* STDOUT */
  {
    putbuf (buffer, size);
    return size;
  }

  struct file_descriptor *desc = fd_to_file_descriptor(fd);

  if (desc == NULL)
    return -1;

  return file_write (desc->file, buffer, size);
}

/* TODO:
  file system lock
  pointer validity check
  Make.tests Line 17: TIMEOUT = 60
  /userprog/no-vm/Make.tests : TIMEOUT = 360 */
