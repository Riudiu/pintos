#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "threads/synch.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

void halt (void) NO_RETURN;
void exit (int status) NO_RETURN;
tid_t fork (const char *thread_name, struct intr_frame *f);
int exec (const char *file);
int wait (tid_t);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

struct file {
	struct inode *inode;        /* File's inode. */
	off_t pos;                  /* Current position. */
	bool deny_write;            /* Has file_deny_write() been called? */
};
struct lock filesys_lock;

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
	
	lock_init(&filesys_lock);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	
	// 함수 return 값에 대한 x86-64 convention은 이 값을 rax레지스터에 배치하는 것이다.
    // 값을 반환하는 system call은 struct int_frame의 rax 멤버를 수정함으로써 convention을 지킨다.
	
	switch (f->R.rax) { // rax is system call number
		case SYS_HALT:
			halt();
			break;
		case SYS_EXIT:
			exit(f->R.rdi); //실행할 때 첫번째 인자가 R.rdi에 저장됨
			break;
		// case SYS_FORK:
		// 	f->R.rax = fork(f->R.rdi, f);
		// 	break;
		// case SYS_EXEC:
		// 	if (exec(f->R.rdi) == -1)
		// 	{
		// 		exit(-1);
		// 	}
		// 	break;
		// case SYS_WAIT:
		// 	f->R.rax = process_wait(f->R.rdi);
		// 	break;
		case SYS_CREATE:
			f->R.rax = create(f->R.rdi, f->R.rsi);
			break;
		case SYS_REMOVE:
			f->R.rax = remove(f->R.rdi);
			break;
		case SYS_OPEN:
			f->R.rax = open(f->R.rdi);
			break;
		case SYS_FILESIZE:
			f->R.rax = filesize(f->R.rdi);
			break;
		case SYS_READ:
			f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_WRITE:
			f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_SEEK:
			seek(f->R.rdi, f->R.rsi);
			break;
		case SYS_TELL:
			f->R.rax = tell(f->R.rdi);
			break;
		case SYS_CLOSE:
			close(f->R.rdi);
			break;
		default:
			exit(-1);
			break;
    }
}

/// Helper Functions
/* 유저 영역에서 사용하는 주소값인지 확인 */
void check_address(void *addr) {
    struct thread *curr = thread_current();
    if (is_kernel_vaddr(addr) || pml4_get_page(curr->pml4, addr) == NULL)
    {
        exit(-1);
    }
}
static struct file *find_file_by_fd(int fd)
{
    struct thread *curr = thread_current();
    if (fd < 0 || fd >= FDCOUNT_LIMIT) {
        return NULL;
    }
    return curr->fd_table[fd];
}
/* Add a file to the fd table of the current process */
int add_file_to_fdt(struct file *file)
{
    struct thread *curr = thread_current();
    struct file **fdt = curr->fd_table;

    // Find open spot from the front
    // fd 위치가 제한 범위 넘지않고, fd table의 인덱스 위치와 일치한다면
    while (curr->fd_idx < FDCOUNT_LIMIT && fdt[curr->fd_idx])
    {
        curr->fd_idx++;
    }

    // error - fd table full
    if (curr->fd_idx >= FDCOUNT_LIMIT) {
        return -1;
	}

    fdt[curr->fd_idx] = file;
    return curr->fd_idx;
}
/* Remove current thread from fd table */
void remove_file_from_fdt(int fd)
{
    struct thread *curr = thread_current();

    // error : invalid fd
    if (fd < 0 || fd >= FDCOUNT_LIMIT) {
        return;
	}

    curr->fd_table[fd] = NULL;
}

/* pintos Exit System Call */
void 
halt (void) {
	power_off();
}

/* Process End System Call */	
void 
exit (int status) {
	struct thread *curr = thread_current();
	curr->exit_status = status;                       // 종료시 상태를 확인, 정상종료면 state = 0
    printf("%s: exit(%d)\n", thread_name(), status);  // 종료 메시지 출력
	thread_exit();
}

/* Create a file. Returns true if file creation is successful and false if it fails.  */
bool
create (const char *file, unsigned initial_size) {
	check_address(file);
	return filesys_create(file, initial_size);
}

/* Delete the file. Returns true if file removal is successful and false if it fails. */
bool 
remove (const char *file) {
	check_address(file);
	return filesys_remove(file);
}

/// File Descriptors
/* Add a file to the fd table and get the fd, 
   return fd if success, return -1 if fail */
int 
open (const char *file) {
	check_address(file);
	struct file *open_file = filesys_open(file);
	
	if (open_file == NULL) {
		return -1;
	}
	// fd table에 file 추가
	int fd = add_file_to_fdt(open_file);
	
	// fd table is full
	if (fd == -1) {
		file_close(open_file);
	}
	return fd;
}

/* Return file size */
int 
filesize (int fd) {
	struct file *open_file = find_file_by_fd(fd);
	if (open_file == NULL) {
		return -1;
	}
	return file_length(open_file);
}

/* Read data from open file */
int 
read (int fd, void *buffer, unsigned size) {
	check_address(buffer);
	off_t read_byte;
	uint8_t *read_buffer = buffer;
	// fd가 0이면 STDIN, 키보드로 들어온 값을 읽는다
	if (fd == 0) {
		char key;
		for (read_byte = 0; read_byte < size; read_byte++) {
			key = input_getc();
			*read_buffer++ = key;
			if (key == '\0') break;
		}
	}
	// fd가 1이면 STDOUT
	else if (fd == 1) {
		return -1;
	}
	// 그외 경우에 open된 file을 찾아서 file을 읽는다
	else {
		struct file *read_file = find_file_by_fd(fd);
		if (read_file == NULL) {
			return -1;
		}
		lock_acquire(&filesys_lock);
		read_byte = file_read(read_file, buffer, size);
		lock_release(&filesys_lock);
	}
	return read_byte;
}

/* Record data from open file */
int 
write (int fd, const void *buffer, unsigned size) {
	check_address(buffer);
	struct file *file = find_file_by_fd(fd);
	int write_result;
	
	lock_acquire(&filesys_lock);
	if (fd == 1) {
		putbuf(buffer, size);  // 문자열을 화면에 출력하는 함수
		write_result = size;
	}
	else {
		if (file != NULL) 
			write_result = file_write(file, buffer, size);
		else 
			write_result = -1;
	}
	lock_release(&filesys_lock);
	return write_result;
}

/* Move the location of the file to position */
void 
seek (int fd, unsigned position) {
	struct file *seek_file = find_file_by_fd(fd);
	if (seek_file <= 2) {  // 0,1,2 is already defined
		return;  
	}
	seek_file->pos = position;
}

/* Tells the current location of the file */
unsigned
tell (int fd) {
	struct file *tell_file = find_file_by_fd(fd);
	if (tell_file <= 2) return;
	return file_tell(tell_file);
}

/* Find the file with fd and delete it from the fd table */
void 
close (int fd) {
	struct file *file = find_file_by_fd(fd);
	if (file == NULL) return;
	remove_file_from_fdt(fd);
}
