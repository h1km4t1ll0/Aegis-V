#ifndef K_H
#define K_H

#define UART0       0x10000000UL
#define UART_LSR    5
#define LSR_TX      0x20

#define AT_FDCWD    -100
#define ENOSYS      38
#define ENOENT      2
#define ENOTTY      25
#define ENOMEM      12
#define EBADF       9
#define EINVAL      22
#define EFAULT      14
#define EEXIST      17

#define O_RDONLY    0
#define O_WRONLY    1
#define O_RDWR      2
#define O_CREAT     64
#define O_TRUNC     512
#define O_APPEND    1024
#define F_GETFL     3
#define MAP_ANONYMOUS 0x20

#define SYS_getcwd  17
#define SYS_fcntl   25
#define SYS_ioctl   29
#define SYS_mkdirat 34
#define SYS_faccessat 48
#define SYS_openat  56
#define SYS_close   57
#define SYS_lseek   62
#define SYS_read    63
#define SYS_write   64
#define SYS_exit    93
#define SYS_uname   160
#define SYS_gettimeofday 169
#define SYS_brk     214
#define SYS_munmap  215
#define SYS_mmap    222
#define SYS_mprotect 226
#define SYS_newfstatat 79
#define SYS_fstat   80
#define SYS_clock_gettime 113

#define MCAUSE_ECALL_U 8
#define MCAUSE_ECALL_M 11

#define S_IFREG 0100000
#define S_IFDIR 0040000

void uart_putc(int c);
void uart_puts(const char *s);
void uart_hex(unsigned long x);
void panic(const char *s);
void *memcpy(void *d, const void *s, unsigned long n);
void *memset(void *d, int c, unsigned long n);
unsigned long strlen(const char *s);

unsigned long kphys(unsigned long hi, unsigned long mid);
unsigned long kphys_dram(void);
unsigned long kphys_ramfs(void);
unsigned long kphys_user(void);
unsigned long kphys_usp(void);
unsigned long kphys_kalloc(void);
unsigned long uva2pa(unsigned long va);
void *kalloc(unsigned long n);
void kmem_init(void);
void vm_init(void);
void kmem_set_end(unsigned long end);
long kmem_brk(unsigned long a);
long kmem_mmap(unsigned long len, unsigned long flags);

void ramfs_init(void *blob);
void *kfs_get(const char *path, unsigned long *len);
int ramfs_open(const char *path, int flags);
long ramfs_read(int fd, void *buf, unsigned long n);
long ramfs_write(int fd, const void *buf, unsigned long n);
long ramfs_lseek(int fd, long off, int whence);
int ramfs_close(int fd);
int ramfs_stat_size(const char *path, unsigned long *sz);
int ramfs_mkdir(const char *path);
int ramfs_exists(const char *path);
int ramfs_isdir(const char *path);
int ramfs_fd_size(int fd, unsigned long *sz);

const char *proc_cwd(void);
unsigned long elf_load(void *blob, unsigned long len);
void enter_user(unsigned long entry, unsigned long sp, char **argv);
void run(char *path, char **argv);
long syscall(long a7, long a0, long a1, long a2, long a3, long a4, long a5);
void trap_c(unsigned long mcause, unsigned long *regs);
unsigned long read_mepc(void);
unsigned long read_mtval(void);
void write_mepc(unsigned long v);
void mret_user(unsigned long entry, unsigned long sp);
void jump_k(void);
void write_satp(unsigned long pt);

#endif
