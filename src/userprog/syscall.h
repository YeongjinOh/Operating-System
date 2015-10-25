#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

static struct lock filesys_lock; // lock for file system.
void syscall_init (void);

#endif /* userprog/syscall.h */
