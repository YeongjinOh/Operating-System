#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include "threads/vaddr.h"
#include "threads/malloc.h"

static void syscall_handler (struct intr_frame *);

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


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_lock);
}


/* check if address is null, null pointer or not in user address space
   if so, call exit(-1) */
void check_valid_address(void *address)  
{
  if(address == NULL || *address == NULL) exit(-1);
  else if(!is_user_vaddr(address)) exit(-1); 
}


/* get n arguments from esp, checking if it is valid
   if not, exit(-1) is called in check_valid_address */
void get_arg(void *esp, void *arg, int n)
{
  int i;
  for(i = 0; i<n; i++)
  {
    check_valid_address(esp);
    arg[i] = *(esp++);
  }
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int nsyscall, ret;
  int *esp = (int *)f->esp;
  int *arg;

  //check esp is valid>
  check_valid_address(esp);
  nsyscall = *(esp++);

  switch(nsyscall)
  {
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      arg = (int *)malloc(1);
      get_arg(esp, arg, 1);
      exit((int)*(int *)arg[0]);
      break;
    case SYS_EXEC:
      arg = (int *)malloc(1);
      get_arg(esp, arg, 1);
      check_valid_address(arg[0]);
      ret = exec((char *)*(int *)arg[0]);
      break;
    case SYS_WAIT:
      arg = (int *)malloc(1);
      get_arg(esp, arg, 1);
      ret = wait((pid_t)*(int *)arg[0]);
      break;	

    case SYS_CREATE:
      arg = (int *)malloc(2);
      get_arg(esp, arg, 2);
      check_valid_address(arg[0]);
      ret = create((char *)*(int *)arg[0], (unsigned)*(int *)arg[1]);
      break;
    case SYS_REMOVE:
      arg = (int *)malloc(1);
      get_arg(esp, arg, 1);
      check_valid_address(arg[0]);
      ret = remove((char *)*(int *)arg[0]);
      break;
    case SYS_OPEN:
      arg = (int *)malloc(1);
      get_arg(esp, arg, 1);
      check_valid_address(arg[0]);
      ret = open((char *)*(int *)arg[0]);
      break;
    case SYS_FILESIZE:
      arg = (int *)malloc(1);
      get_arg(esp, arg, 1);
      ret = filesize((int)*(int *)arg[0]);
      break;
    case SYS_READ:
      arg = (int *)malloc(2);
      get_arg(esp, arg, 2);
      check_valid_address(arg[1]);
      ret = read((int)*(int *)arg[0], (void *)*(int *)arg[1], (unsigned)*(int *)arg[2]);
      break;
    case SYS_WRITE:
      arg = (int *)malloc(2);
      get_arg(esp, arg, 2);
      check_valid_address(arg[1]);
      ret = write((int)*(int *)arg[0], (void *)*(int *)arg[1], (unsigned)*(int *)arg[2]);
      break;
    case SYS_SEEK:
      arg = (int *)malloc(2);
      get_arg(esp, arg, 2);
      seek((int)*(int *)arg[0], (unsigned)*(int *)arg[1]);
      break;
    case SYS_TELL:
      arg = (int *)malloc(1);
      get_arg(esp, arg, 1);
      ret = tell((int)*(int *)arg[0]);
      break;
    case SYS_CLOSE:
      arg = (int *)malloc(1);
      get_arg(esp, arg, 1);
      close((int)*(int *)arg[0]);
      break;
  }  
  f->eax = ret;
  free(arg);
}

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



