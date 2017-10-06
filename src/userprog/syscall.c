#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

#include <string.h>
#include <stdlib.h>
 
extern struct lock lock;

static void syscall_handler (struct intr_frame *);
static void syscall_exit (struct intr_frame *f UNUSED, bool status);
static void syscall_write (struct intr_frame *f UNUSED);
static int get_user (const uint8_t *uaddr);

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

  //Check whether stack pointer exceed PHYS_BASE
  for(i = 0; i < 4; i++){ //Argument of syscall is no more than 3
    if( ( get_user((uint8_t *)syscall_nr + 4 * i ) == -1 )
        || ((void *)((uint8_t *)syscall_nr + 4 * i) >= PHYS_BASE) ){
      syscall_exit(f, false);
    }
  }
  void **arg1 = (void **)(syscall_nr+1);
  //void **arg2 = (void **)(syscall_nr+2);
  //void **arg3 = (void **)(syscall_nr+3);
  //void **arg4 = (void **)(syscall_nr+4);
  switch (*syscall_nr) {
    case SYS_HALT: //0
      power_off();
      break;
  	case SYS_EXIT: //1
      syscall_exit(f, true);
  		break;
    case SYS_EXEC: //2
      printf("cmd_line = %s\n",*(char **)arg1);
      process_execute(*(char **)arg1);
      break;
  	case SYS_WAIT: //3
      process_wait(*(tid_t *)arg1);
  		break; 
    case SYS_CREATE: //4
      // Not implemented
      break;
    case SYS_REMOVE: //5
      // Not implemented
      break;
    case SYS_OPEN: //6
      // Not implemented
      break;
    case SYS_FILESIZE: //7
      // Not implemented
      break;
    case SYS_READ: //8
      // Not implemented
      break;
  	case SYS_WRITE: //9
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

static int get_user (const uint8_t *uaddr){
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
      : "=&a" (result) : "m" (*uaddr));
  return result;
}
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
