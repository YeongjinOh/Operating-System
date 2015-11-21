#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

struct file_elem
{
  int fd;				// file descriptor
  bool isdir;				// is directory
  struct file *file;			// pointer to file
  struct dir *dir; 			// pointer to dir
  struct list_elem thread_elem;		// list element for thread's 'files' list
};

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
#endif /* userprog/process.h */
