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
#include "devices/input.h"

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
int alloc_fd(void);

static struct lock filesys_lock;  // lock for file system

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
  struct thread *t = thread_current();
  //printf("userprog/syscall.c	check_valid_address\n");  
  if(!address || !is_user_vaddr(address) || pagedir_get_page (t->pagedir, address) == NULL) exit(-1);
  //else if(!is_user_vaddr(*(char *)address)) exit(-1);
  return;
}


static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  ASSERT (f!=NULL);
 // printf("userprog/syscall.c	syscall_handler\n");  
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
      check_valid_address((int *)(esp+1));
      exit(*(int *)(esp+1));
      break;
    case SYS_EXEC:
      check_valid_address((char *)*(esp+1));
      ret = exec((char *)*(esp+1));
      break;
    case SYS_WAIT:
      check_valid_address((pid_t *)(esp+1));
      ret = wait(*(esp+1));
      break;	

    case SYS_CREATE:
      check_valid_address((char *)*(esp+1));
      check_valid_address((unsigned *)(esp+2));
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
      check_valid_address((int *)(esp+1));
      ret = filesize(*(esp+1));
      break;
    case SYS_READ:
      check_valid_address((int *)(esp+1));
      check_valid_address((char *)*(esp+2));
      check_valid_address((unsigned *)(esp+3));
      ret = read(*(esp+1), (char *)*(esp+2), *(esp+3));
      break;
    case SYS_WRITE:
      check_valid_address((int *)(esp+1));
      check_valid_address((char *)*(esp+2));
      check_valid_address((unsigned *)(esp+3));
      ret = write(*(esp+1), (char *)*(esp+2), *(esp+3));
      break;
    case SYS_SEEK:
      check_valid_address((int *)(esp+1));
      check_valid_address((unsigned *)(esp+2));
      seek(*(esp+1), *(esp+2));
      break;
    case SYS_TELL:
      check_valid_address((int *)(esp+1));
      ret = tell(*(esp+1));
      break;
    case SYS_CLOSE:
      check_valid_address((int *)(esp+1));
      close(*(esp+1));
      break;

    default:
      exit(-1);
      break;
  }  
  f->eax = ret;
}

/* Process System Calls */

void halt (void)
{
  //printf("userprog/syscall.c	halt\n");  
  shutdown_power_off();
}

void exit (int status)
{
  //printf("userprog/syscall.c	exit\n");  
  struct thread *t = thread_current();
  t->exit_status = status;
  printf ("%s: exit(%d)\n",t->name,status);
  thread_exit ();
}

pid_t exec (const char *file)
{
  //printf("userprog/syscall.c	exec\n");  
  pid_t pid;
  check_valid_address(file);
  lock_acquire (&filesys_lock);  /* Do not allow file IO until process is loaded */
  pid = process_execute (file);
  lock_release (&filesys_lock);
  return pid;
}

int wait (pid_t pid)
{
  //printf("userprog/syscall.c	wait\n");  
  return process_wait (pid);
}



/* File System Calls */

// when it cannot be written, exit(-1) or written = 0 or written = -1?
int write (int fd, const void *buffer, unsigned length)
{
  //printf("userprog/syscall.c	Write / fd : %d, buffer : %x, length : %d\n",fd,buffer,length);
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
  } else  // write to a file
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


bool create (const char *file, unsigned initial_size)
{
  //printf("userprog/syscall.c	create\n");  
  bool ret;
  if(!file) exit(-1);
  else
  {
    lock_acquire(&filesys_lock);
    ret = filesys_create(file, initial_size);
    lock_release(&filesys_lock);
  }
  return ret;
}


bool remove (const char *file)
{
  //printf("userprog/syscall.c	remove\n");  
  bool ret;
  if(!file) exit(-1);
  else
  {
    lock_acquire(&filesys_lock);
    ret = filesys_remove(file);
    lock_release(&filesys_lock);
  }
  return ret;
}


//open file
int open (const char *file)
{
  //printf("userprog/syscall.c	open\n");  
  struct file *f;
  struct file_elem *fe;
  
  if(!file) return -1; // input name is null
  
  lock_acquire(&filesys_lock);
  f = filesys_open(file);
  //lock_release(&filesys_lock);

  if(!f) return -1; // fail to open file

  fe = (struct file_elem *)malloc(sizeof(struct file_elem));

  if(!fe) // fail to allocate memory
  {
    file_close(f);
    lock_release(&filesys_lock);
    return -1; 
  }

  fe->fd = alloc_fd();
  fe->file = f;
  list_push_back(&thread_current()->files, &fe->thread_elem);
  lock_release(&filesys_lock);

  return fe->fd;
}


/* returns allocated fd value */
int alloc_fd(void)
{
  static int fd = 2;
  return fd++;
}

/* returns file size */
int filesize (int fd)
{
  struct file_elem *fe = find_file_elem(fd);
  if(!fe) exit(-1);
  return file_length(fe->file);
}

/* returns the number of bytes actually read 
   returns -1 if it could not be read */
int read (int fd, void *buffer, unsigned length)
{
  //printf("userprog/syscall.c	read\n");  
  int ret = 0;
  struct file_elem *fe;
  unsigned i;

  if(!is_user_vaddr(buffer)||(!is_user_vaddr(buffer+length))) return -1; // buffer is not in user virtual address
  
  if(fd == 0)  //stdin
  {
    for(i=0; i<length; i++)
    {
      *(uint8_t *)(buffer + i) = input_getc();
    }
    ret = length;
  } else if(fd == 1) return -1; // stdout
  else
  {
    lock_acquire(&filesys_lock);
    fe = find_file_elem(fd);
    if(!fe)
    { 
      lock_release(&filesys_lock);
      return -1;
    }
    ret = file_read(fe->file, buffer, length);
    lock_release(&filesys_lock);
  }

  return ret;
}


void seek (int fd, unsigned position)
{
  //printf("userprog/syscall.c	seek\n");  
  struct file_elem *fe = find_file_elem(fd);
  if(!fe) exit(-1); // if the file could not be found, call exit(-1)
  struct file *f = fe->file;
  lock_acquire(&filesys_lock);
  file_seek(f, position);
  lock_release(&filesys_lock);
}


unsigned tell (int fd)
{ 
  //printf("userprog/syscall.c	tell\n");  
  unsigned ret;
  struct file_elem *fe = find_file_elem(fd);
  if(!fe) exit(-1); // if the file could not be found, call exit(-1)
  struct file *f = fe->file;
  lock_acquire(&filesys_lock);
  ret = file_tell(f);
  lock_release(&filesys_lock);

  return ret;
}

void close (int fd)
{
  //printf("userprog/syscall.c	close\n");  
  struct file_elem *fe = find_file_elem(fd);
  if(!fe) exit(-1); // if the file could not be found, call exit(-1)
  struct file *f = fe->file;
  
  lock_acquire(&filesys_lock);
  file_close(f);
  list_remove(&fe->thread_elem);
  lock_release(&filesys_lock);
  
  free(fe);
}

