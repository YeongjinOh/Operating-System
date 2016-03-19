# Team01

Pintos project based on Operating System class in SNU.
Our team's private repository is destroyed, so move it here in order to conserve source files.


----- Pintos Project 0 -----

* Pintos Test

Check out the build and execution
 - cd pintos/src/threads
 - make
 - ls build

check out generated 'kernel.bin kernel.o' files
 - cd build
 - ../../utils/pintos -- -q run alarm-single (runitem)

Test the project
 - cd pintos/src/threads/build (gop)
 - ../../utils/pintos -- -q run alarm-single (runitem)  		 single execution
 - make tests/threads/alarm-single.result (checkitem)  	 single test
 - make check		 test all

Checking the score
 - make grade 		 generates 'grade' file
 - “Run didn't start up properly: no "Pintos booting" message”
 - bochs-2.6.2/main.cc : MUST delete 'bx_print_header()' in bx_init_main()

* Debug

Backtrace
 - backtrace kernel.o 0xc0106eff 0xc01102fb ...

GDB
 - pintos --gdb -- run alarm-single (debugitem)
 - <new terminal>
 - pintos-gdb kernel.o (debugpintos)
 - (gdb) target remote localhost:1234 (debugpintos)

GDB commands
 - b, bt, c, f, n, p, s, x
 - Refer to 'src/misc/gdb-macros'

Log
 - Should use 'printf'


----- Pintos Project 1 -----

1. Wait Queue
When a thread goes to sleep state, make Pintos use wait queue (instead of busy wait)
It will be used in 'filesys' project

timer_sleep() in devices/timer.c
 - Suspends execution of the calling thread until time has advanced at least x timer ticks
	 The time unit is tick (100 ticks occur per second)
	※ The time unit is not a second
 - In original Pintos, timer_sleep() is implemented by Busy Wait
	The thread spins in a loop checking the current time and calling thread_yield() until enough time has gone by


Re-implement it to avoid busy waiting
 - When thread_sleep() is called
	thread block  wait queue
 - When the timer is elapsed
	thread awake  ready queue


2. Priority Scheduling
Implement priority scheduling
The thread that has higher priority should run first (sort a ready queue according to the priority)
Solve priority-inversion problem by priority-donation

ready_queue
 - When a scheduler pushes threads to ready queue, should sort the threads according to priority
 - When priority is changed or a thread is created or terminated
	Change the running thread immediately according to priority
priority donation
 - Modify 'lock' in synch.c and synch.h


----- Pintos Project 2 -----


1. Use File System

1) Format the disk
 - pintos -f –q
2) Copy a file and execute it
 - pintos -p <filename> -a <newname> -- -q
 - pintos -q run '<newname> arg'
3) Setup aliases
 - export TEST_EMUL='qemu'
 - PINTOS_PRJ='userprog'
 - TEST_DIR='userprog'

2. Load User Program
1) Argument passing (process.c)
 - Modify 'start_process (void *file_name_)'
	Modify it so as to be able to pass not only 'file_name_', but also command line arguments (argc and argv)
 - Parse command line arguments
	strtok_r ()
 - Push command line arguments onto a stack according to 'x86 Calling Convention'
	4 byte-aligned access
	Stack pointer: if_.esp
	Modify *esp = PHYS_BASE in setup_stack() to *esp = PHYS_BASE – 12 to get started

3. System call
1) System call handler (syscall.c)
 - Put a return value in f->eax when it is terminated
 - Verify pointer values which a user inputs

static void syscall_handler(structintr_frame*f)
{
    int nsyscall, ret;
    int *esp = (int *)f->esp;
    <check esp is valid>
    nsyscall = *(esp++);
    …
    f->eax = ret;
}


Process Management
 - Define state
 - TASK_STOPPED, TASK_RUNNING, TASK_READY, TASK_ZOMBIE, etc.
 - Design process relationship

Process Termination Message
 - Whenever a user process terminates, print the process’s name and exit code
   	ex) printf("%s: exit(%d)\n", …)


2) Process system calls (Manual p.29~)
 - void halt (void)
 - void exit (int status)
	Should be implemented first
 - pid_t exec (const char *cmd_line)
 - int wait (pid t pid)
	VERY HARD

File descriptors management
 - struct file: private file information (file offset etc.)
 - struct inode: shared file information (owner, pointer to sectors etc.)

Design file descriptors management
 - 0, 1, 2 : pre-defined fd
 - Read : fd = STDIN_FILENO(0)  input_getc()
 - Write : fd = STDOUT_FILENO(1)  putbuf()

Deny writes to executing files
 - file-> deny_write, inode->deny_write_cnt
 - When a file is opened, call file_deny_write()
 - When a file is closed, call file_allow_write()

3) File system calls (Manual p.30~)
 - Manage file descriptors
 - Refer to filesys.h and file.h
 - bool create (const char *file, unsigned initial_size)
 - bool remove (const char *file)
 - int open (const char *file)
 - int filesize (int fd)
 - int write (int fd, const void *buffer, unsigned size)
 - Should be implemented first
 - void seek (int fd, unsigned position)
 - unsigned tell (int fd)
 - void close (int fd)


----- Pintos Project 4 -----

1. Extensible Files

Current Pintos
 - The file system allocates files as a single extent
Modify the file system
 - A file is initially created with size 0
 - Expand the file every time a write is made off the end of the file
 - Partition size : <= 8MB
 - Allowed to seek beyond the current EOF
 - Writing at a position past EOF extends the file to the position

2. Subdirectories

Current Pintos
 - All files live in a single directory
 - 14-character limit on file names
Implement a hierarchical name space
 - Allow directory entries to point to files or to other directories.
 - Must allow full path names > 14 characters
Maintain a current directory for each process
 - At startup, set the root as the current directory
 - exec system call make the child process inherits its parent's current directory
Path name
 - An absolute or relative path name may used
 - The directory separator character: '/‘
 - Must also support special file names '.' and '..'

Update the system calls (pp. 31)
 - int open (const char *file)
 - void close (int fd)
 - bool remove (const char *file)

Implement the new system calls (pp. 53)
 - bool chdir (const char *dir)
 - bool mkdir (const char *dir)
 - bool readdir (int fd, char *name)
 - bool isdir (int fd)
 - int inumber (int fd)


