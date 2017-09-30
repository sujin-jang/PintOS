#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "threads/synch.h"

#include <string.h>
#include <stdlib.h>

extern struct lock lock;

static void syscall_handler (struct intr_frame *);
static void syscall_exit (struct intr_frame *f UNUSED);
static void syscall_write (struct intr_frame *f UNUSED);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  // Haney: Why size_t?
  //size_t *syscall_nr = (size_t *)(f->esp);
  int *syscall_nr = (int *)(f->esp);
  switch (*syscall_nr) {
    case SYS_HALT:
      power_off();
      break;
  	case SYS_EXIT:
      syscall_exit(f);
  		break;
    case SYS_EXEC:
      // const char *cmd_line = (const char *)(syscall_nr+1);
      // Not implemented
      break;
  	case SYS_WAIT:
      // tid_t tid = (tid_t)(*(syscall_nr + 1));
      //process_wait(tid);
  		break; 
    case SYS_CREATE:
      // Not implemented
      break;
    case SYS_REMOVE:
      // Not implemented
      break;
    case SYS_OPEN:
      // Not implemented
      break;
    case SYS_FILESIZE:
      // Not implemented
      break;
    case SYS_READ:
      // Not implemented
      break;
  	case SYS_WRITE:
  		syscall_write(f);
  		break;
    case SYS_SEEK:
      // Not implemented
      break;
    case SYS_TELL:
      // Not implemented
      break;
    case SYS_CLOSE:
      // Not implemented
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
static void
syscall_exit (struct intr_frame *f UNUSED)
{
  int *syscall_nr = (int *)(f->esp);
  struct thread *curr = thread_current();
  struct thread *parent = curr->parent;
  struct list_elem* e;
  struct child *child_process;
  for(e = list_begin(&parent->child);
      e != list_end (&parent->child);
      e = list_next (e)){
    child_process = list_entry(e, struct child, elem);
    if (child_process->tid == curr->tid)
      child_process->exit_stat = *(syscall_nr + 1);
  }
  printf("%s: exit(%d)\n", curr->name, *(syscall_nr + 1));
  lock_release(&lock);
  thread_exit();
}

static void
syscall_write (struct intr_frame *f UNUSED)
{
 	/* if fd == 1 */
 	/* STDOUT: write to console */

 	size_t *size = (size_t *)(f->esp + 0X0C);
 	size_t *buf_temp = (size_t *)(f->esp + 0X08);
 	const char * buf = (const char *)(*buf_temp);

 	//printf("%#010x\n", *size);
 	//printf("%#010x\n", buf);
 	putbuf (buf, *size);
}
