#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/synch.h"

static void syscall_handler (struct intr_frame *);

typedef int pid_t;

struct file_elem
{
  int fd;				// file descriptor
  struct file *file;			// pointer to file
  struct list_elem thread_elem;		// list element for thread's 'files' list
};

// Process System Calls 
void halt (void) NO_RETURN;
void exit (int status) NO_RETURN;
pid_t exec (const char *file);
int wait (pid_t);

// File System Calls
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned length);
int write (int fd, const void *buffer, unsigned length);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

void check_valid_address(void *address);  
struct file_elem * find_file_elem(int fd);

static struct lock filesys_lock;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_lock);
}


/* check if address is null pointer or not in user address space
   if so, call exit(-1) */
void check_valid_address(void *address)  
{
  if(address) exit(-1);
  else if(!is_user_vaddr(address)) exit(-1);
  return;
}


static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int nsyscall, ret;
  int *esp = (int *)f->esp;

  //check esp is valid>
  check_valid_address(esp);
  nsyscall = *esp;

  switch(nsyscall)
  {
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      exit(*(esp+1));
      break;
    case SYS_EXEC:
      check_valid_address((char *)*(esp+1));
      ret = exec((char *)*(esp+1));
      break;
    case SYS_WAIT:
      ret = wait(*(esp+1));
      break;	

    case SYS_CREATE:
      check_valid_address((char *)*(esp+1));
      ret = create((char *)*(esp+1), *(esp+2));
      break;
    case SYS_REMOVE:
      check_valid_address((char *)*(esp+1));
      ret = remove((char *)*(esp+1));
      break;
    case SYS_OPEN:
      check_valid_address((char *)*(esp+1));
      ret = open((char *)*(esp+1));
      break;
    case SYS_FILESIZE:
      ret = filesize(*(esp+1));
      break;
    case SYS_READ:
      check_valid_address((char *)*(esp+2));
      ret = read(*(esp+1), (char *)*(esp+2), *(esp+3));
      break;
    case SYS_WRITE:
      check_valid_address((char *)*(esp+2));
      ret = write(*(esp+1), (char *)*(esp+2), *(esp+3));
      break;
    case SYS_SEEK:
      seek(*(esp+1), *(esp+2));
      break;
    case SYS_TELL:
      ret = tell(*(esp+1));
      break;
    case SYS_CLOSE:
      close(*(esp+1));
      break;
  }  
  f->eax = ret;
}

/* Process System Calls */



/* File System Calls */

// when it cannot be written, exit(-1) or written = 0 or written = -1?
int write (int fd, const void *buffer, unsigned length)
{
  int written = 0;
  if(fd == 0) exit(-1);// write to input (error)
  else if(fd == 1)  // write to console
  {
    lock_acquire(&filesys_lock);
    if(length < 512)
    {
      putbuf((char *)buffer, length);
      written = length;
    } else
    {
      while(length>512)
      {
        putbuf((char *)(buffer+written), 512);
	length -= 512;
	written += 512;
      }
      putbuf((char *)(buffer+written), length);
      written += length;
    }
    lock_release(&filesys_lock);
  } else if(fd == 2) exit(-1); // stderr
  else  // write to a file
  {
    struct file_elem *fe = find_file_elem(fd);
    if(fe==NULL) exit(-1);
    else
    {
      struct file *f = fe->file;
      lock_acquire(&filesys_lock);
      written = file_write(f, buffer, length);
      lock_release(&filesys_lock);
    }
  }

  return written;
}

// find a file_elem by fd
struct file_elem * find_file_elem(int fd)
{
  struct list_elem *e;
  struct thread *t = thread_current();

  for(e = list_begin (&t->files); e != list_end (&t->files); e = list_next (e))
  {
    struct file_elem *fe = list_entry (e, struct file_elem, thread_elem);
    if (fe->fd == fd) return fe;
  }
  return NULL;
}


