#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include <string.h>
#include <stdlib.h>

static void syscall_handler (struct intr_frame *);
static void syscall_write (struct intr_frame *f UNUSED);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call!\n");
  hex_dump(f->esp, f->esp, 0X30, true);

  size_t *syscall_nr = (size_t *)(f->esp);

  switch (*syscall_nr) {
  	case SYS_EXIT: 
  		thread_exit();
  		break;
  	case SYS_WAIT:
  		break; 
  	case SYS_WRITE:
  		syscall_write(f);
  		break;
  }
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
