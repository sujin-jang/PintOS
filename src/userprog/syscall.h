#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

struct lock lock_file;

void syscall_init (void);
void syscall_exit (int status);

#endif /* userprog/syscall.h */
