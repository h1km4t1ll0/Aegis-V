/* libc for tcc-pnut self-host of stages/08-tcc. FILE* is a disguised fd. */
#define PNUT_USE_BRK 1
int errno;
int open(char *path, int flags, int mode);
int read(int fd, char *buf, int n);
int write(int fd, char *buf, int n);
int close(int fd);
int lseek(int fd, int off, int whence);
void exit(int n);
char *brk(char *addr);

struct tm {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
};

struct timeval {
	long tv_sec;
	long tv_usec;
};

struct tm pnut_tm;

void *stdin = 0;
void *stdout = (void *)1;
void *stderr = (void *)2;

char *memcpy(char *d, char *s, int n)
{
	int i;
	i = 0;
	while (i < n)
	{
		d[i] = s[i];
		i = i + 1;
	}
	return d;
}

char *memmove(char *d, char *s, int n)
{
	int i;
	if (d < s)
	{
		i = 0;
		while (i < n)
		{
			d[i] = s[i];
			i = i + 1;
		}
	}
	else
	{
		i = n;
		while (i > 0)
		{
			i = i - 1;
			d[i] = s[i];
		}
	}
	return d;
}

char *memset(char *d, int c, int n)
{
	int i;
	i = 0;
	while (i < n)
	{
		d[i] = c;
		i = i + 1;
	}
	return d;
}

int memcmp(char *a, char *b, int n)
{
	int i;
	i = 0;
	while (i < n)
	{
		if (a[i] != b[i])
		{
			return (a[i] & 255) - (b[i] & 255);
		}
		i = i + 1;
	}
	return 0;
}

int strlen(char *s)
{
	int n;
	n = 0;
	while (s[n] != 0)
	{
		n = n + 1;
	}
	return n;
}

char *strcpy(char *d, char *s)
{
	int i;
	i = 0;
	while (s[i] != 0)
	{
		d[i] = s[i];
		i = i + 1;
	}
	d[i] = 0;
	return d;
}

char *strncpy(char *d, char *s, int n)
{
	int i;
	i = 0;
	while (i < n && s[i] != 0)
	{
		d[i] = s[i];
		i = i + 1;
	}
	while (i < n)
	{
		d[i] = 0;
		i = i + 1;
	}
	return d;
}

int strcmp(char *a, char *b)
{
	int i;
	i = 0;
	while (a[i] != 0 && a[i] == b[i])
	{
		i = i + 1;
	}
	return (a[i] & 255) - (b[i] & 255);
}

int strncmp(char *a, char *b, int n)
{
	int i;
	i = 0;
	while (i < n)
	{
		if (a[i] != b[i] || a[i] == 0)
		{
			return (a[i] & 255) - (b[i] & 255);
		}
		i = i + 1;
	}
	return 0;
}

char *strcat(char *d, char *s)
{
	return strcpy(d + strlen(d), s);
}

char *strchr(char *s, int c)
{
	while (s[0] != 0)
	{
		if ((s[0] & 255) == (c & 255))
		{
			return s;
		}
		s = s + 1;
	}
	if ((c & 255) == 0)
	{
		return s;
	}
	return 0;
}

char *strrchr(char *s, int c)
{
	char *last;
	last = 0;
	while (s[0] != 0)
	{
		if ((s[0] & 255) == (c & 255))
		{
			last = s;
		}
		s = s + 1;
	}
	if ((c & 255) == 0)
	{
		return s;
	}
	return last;
}

char *strstr(char *h, char *n)
{
	int nl;
	if (n[0] == 0)
	{
		return h;
	}
	nl = strlen(n);
	while (h[0] != 0)
	{
		if (strncmp(h, n, nl) == 0)
		{
			return h;
		}
		h = h + 1;
	}
	return 0;
}

char pnut_dummy[8];
#ifndef PNUT_USE_BRK
char pnut_arena[33554432];
#else
char *pnut_arena;
int pnut_arena_inited;
char *brk(char *addr);
#endif
int pnut_arena_off = 0;

char *malloc(int n)
{
	char *p;
#ifdef PNUT_USE_BRK
	if (pnut_arena_inited == 0)
	{
		pnut_arena = brk(0);
		brk(pnut_arena + 33554432);
		pnut_arena_inited = 1;
	}
#endif
	if (n < 1)
	{
		n = 1;
	}
	n = n + 7;
	n = (n / 8) * 8;
	if (pnut_arena_off + n > 33554432)
	{
		return 0;
	}
	p = pnut_arena + pnut_arena_off;
	pnut_arena_off = pnut_arena_off + n;
	return p;
}

char *calloc(int a, int b)
{
	char *p;
	int n;
	int i;
	n = a * b;
	p = malloc(n);
	if (p == 0)
	{
		return 0;
	}
	i = 0;
	while (i < n)
	{
		p[i] = 0;
		i = i + 1;
	}
	return p;
}

char *realloc(char *p, int n)
{
	char *q;
	int i;
	q = malloc(n);
	if (q == 0)
	{
		return 0;
	}
	if (p == 0)
	{
		return q;
	}
	i = 0;
	while (i < n)
	{
		q[i] = p[i];
		i = i + 1;
	}
	return q;
}

void free(char *p)
{
}

void abort(void)
{
	exit(1);
}

int atoi(char *s)
{
	int n;
	int sign;
	n = 0;
	sign = 1;
	if (s[0] == '-')
	{
		sign = 0 - 1;
		s = s + 1;
	}
	while (s[0] >= '0' && s[0] <= '9')
	{
		n = n * 10 + (s[0] - '0');
		s = s + 1;
	}
	return n * sign;
}

char *getenv(char *name)
{
	return 0;
}

int pnut_strlen(char *s)
{
	int n;
	n = 0;
	while (s[n] != 0)
	{
		n = n + 1;
	}
	return n;
}

int printf(char *fmt, ...)
{
	return write(1, fmt, pnut_strlen(fmt));
}

int fprintf(int f, char *fmt, ...)
{
	if (f < 1)
	{
		f = 1;
	}
	return write(f, fmt, pnut_strlen(fmt));
}

int sprintf(char *s, char *fmt, ...)
{
	if (s)
	{
		s[0] = 0;
	}
	return 0;
}

int snprintf(char *s, int n, char *fmt, ...)
{
	if (s && n > 0)
	{
		s[0] = 0;
	}
	return 0;
}

int vsnprintf(char *s, int n, char *fmt, int ap)
{
	if (s && n > 0)
	{
		s[0] = 0;
	}
	return 0;
}

int fopen(char *path, char *mode)
{
	int flags;
	int fd;
	flags = 0;
	if (mode)
	{
		if (mode[0] == 'w')
		{
			flags = 577;
		}
		if (mode[0] == 'a')
		{
			flags = 577;
		}
	}
	fd = open(path, flags, 511);
	if (fd < 0)
	{
		return 0;
	}
	return fd;
}

int fdopen(int fd, char *mode)
{
	return fd;
}

int fileno(int f)
{
	return f;
}

int freopen(char *path, char *mode, int f)
{
	return f;
}

int dup(int fd)
{
	return fd;
}

int dup2(int a, int b)
{
	return b;
}

int isatty(int fd)
{
	return 0;
}

char *fgets(char *s, int n, int f)
{
	if (s && n > 0)
	{
		s[0] = 0;
	}
	return 0;
}

int ferror(int f)
{
	return 0;
}

int feof(int f)
{
	return 0;
}

void clearerr(int f)
{
}

int fgetc(int f)
{
	return 0 - 1;
}

int getchar(void)
{
	return 0 - 1;
}

int putchar(int c)
{
	char b[1];
	b[0] = c;
	write(1, b, 1);
	return c;
}

int puts(char *s)
{
	write(1, s, pnut_strlen(s));
	putchar(10);
	return 0;
}

int fclose(int f)
{
	return close(f);
}

int fread(char *p, int sz, int n, int f)
{
	int r;
	if (sz < 1)
	{
		return 0;
	}
	r = read(f, p, sz * n);
	if (r < 0)
	{
		return 0;
	}
	return r / sz;
}

int fwrite(char *p, int sz, int n, int f)
{
	int r;
	if (sz < 1)
	{
		return 0;
	}
	if (f < 1)
	{
		f = 1;
	}
	r = write(f, p, sz * n);
	if (r < 0)
	{
		return 0;
	}
	return r / sz;
}

int fseek(int f, int off, int whence)
{
	if (lseek(f, off, whence) < 0)
	{
		return 0 - 1;
	}
	return 0;
}

int ftell(int f)
{
	return lseek(f, 0, 1);
}

int fflush(int f)
{
	return 0;
}

int fputc(int c, int f)
{
	char b[1];
	b[0] = c;
	if (f < 1)
	{
		f = 1;
	}
	write(f, b, 1);
	return c;
}

int fputs(char *s, int f)
{
	if (f < 1)
	{
		f = 1;
	}
	return write(f, s, pnut_strlen(s));
}

int getc(int f)
{
	return 0 - 1;
}

int ungetc(int c, int f)
{
	return c;
}

void qsort(char *base, int nmemb, int size, int (*cmp)(char *, char *))
{
	int i;
	int j;
	int k;
	char tmp;
	char *a;
	char *b;
	if (nmemb < 2 || size < 1)
	{
		return;
	}
	i = 0;
	while (i < nmemb)
	{
		j = 0;
		while (j < nmemb - 1)
		{
			a = base + j * size;
			b = base + (j + 1) * size;
			if (cmp(a, b) > 0)
			{
				k = 0;
				while (k < size)
				{
					tmp = a[k];
					a[k] = b[k];
					b[k] = tmp;
					k = k + 1;
				}
			}
			j = j + 1;
		}
		i = i + 1;
	}
}

int unlink(char *path)
{
	return 0;
}

long strtol(char *s, char **end, int base)
{
	return atoi(s);
}

unsigned long strtoul(char *s, char **end, int base)
{
	return atoi(s);
}

long strtoll(char *s, char **end, int base)
{
	return atoi(s);
}

unsigned long strtoull(char *s, char **end, int base)
{
	char *p;
	unsigned long n;
	int digit;
	p = s;
	n = 0;
	if (base == 0)
	{
		base = 10;
		if (p[0] == '0')
		{
			p = p + 1;
			if (p[0] == 'x' || p[0] == 'X')
			{
				base = 16;
				p = p + 1;
			}
			else
			{
				base = 8;
			}
		}
	}
	while (1)
	{
		if (p[0] >= '0' && p[0] <= '9')
		{
			digit = p[0] - '0';
		}
		else if (p[0] >= 'a' && p[0] <= 'z')
		{
			digit = p[0] - 'a' + 10;
		}
		else if (p[0] >= 'A' && p[0] <= 'Z')
		{
			digit = p[0] - 'A' + 10;
		}
		else
		{
			break;
		}
		if (digit >= base)
		{
			break;
		}
		n = n * base + digit;
		p = p + 1;
	}
	if (end)
	{
		*end = p;
	}
	return n;
}

int fork(void)
{
	return 0 - 1;
}

int execvp(char *file, char **argv)
{
	return 0 - 1;
}

int execv(char *file, char **argv)
{
	return 0 - 1;
}

int waitpid(int pid, int *st, int opt)
{
	return 0 - 1;
}

int wait(int *st)
{
	return 0 - 1;
}

char *realpath(char *path, char *resolved)
{
	return 0;
}

int stat(char *path, int *buf)
{
	return 0 - 1;
}

int fstat(int fd, int *buf)
{
	return 0 - 1;
}

int lstat(char *path, int *buf)
{
	return 0 - 1;
}

int chdir(char *path)
{
	return 0;
}

int rmdir(char *path)
{
	return 0;
}

int putenv(char *s)
{
	return 0;
}

int setenv(char *n, char *v, int o)
{
	return 0;
}

void *opendir(char *path)
{
	return 0;
}

int closedir(void *d)
{
	return 0;
}

char *readdir(void *d)
{
	return 0;
}

int pipe(int *p)
{
	return 0 - 1;
}

int ftruncate(int fd, int n)
{
	return 0;
}

int mkstemp(char *t)
{
	return 0 - 1;
}

int tmpfile(void)
{
	return 0;
}

char *tmpnam(char *s)
{
	return 0;
}

int popen(char *cmd, char *mode)
{
	return 0;
}

int pclose(int f)
{
	return 0;
}

int vprintf(char *fmt, int ap)
{
	return 0;
}

int abs(int n)
{
	if (n < 0)
	{
		return 0 - n;
	}
	return n;
}

long labs(long n)
{
	if (n < 0)
	{
		return 0 - n;
	}
	return n;
}

double ldexp(double x, int exp)
{
	return x;
}

double frexp(double x, int *exp)
{
	if (exp)
	{
		*exp = 0;
	}
	return x;
}

double fabs(double x)
{
	return x;
}

double strtod(char *s, char **end)
{
	if (end)
	{
		*end = s;
	}
	return 0;
}

float strtof(char *s, char **end)
{
	return 0;
}

long double strtold(char *s, char **end)
{
	return 0;
}

double atof(char *s)
{
	return 0;
}

int time(int *t)
{
	return 0;
}

int gettimeofday(int *tv, int *tz)
{
	if (tv)
	{
		tv[0] = 0;
		tv[1] = 0;
		tv[2] = 0;
		tv[3] = 0;
	}
	return 0;
}

char *getcwd(char *buf, int size)
{
	if (buf && size > 1)
	{
		buf[0] = '/';
		buf[1] = 0;
	}
	return buf;
}

void *mmap(void *addr, int len, int prot, int flags, int fd, int off)
{
	return 0;
}

int munmap(void *addr, int len)
{
	return 0;
}

int mprotect(void *addr, int len, int prot)
{
	return 0;
}

int sigsetjmp(int buf, int savemask)
{
	return 0;
}

void longjmp(int *buf, int val)
{
	if (val == 0)
	{
		val = 1;
	}
	exit(val);
}

void *dlopen(char *path, int flags)
{
	return 0;
}

void *dlsym(void *h, char *name)
{
	return 0;
}

int dlclose(void *h)
{
	return 0;
}

char *dlerror(void)
{
	return 0;
}

int remove(char *path)
{
	return 0;
}

int rename(char *a, char *b)
{
	return 0;
}

int system(char *cmd)
{
	return 0;
}

int *localtime(int *t)
{
	return &pnut_tm;
}

int *gmtime(int *t)
{
	return &pnut_tm;
}

int setjmp(int *buf)
{
	return 0;
}

int sscanf(char *s, char *fmt)
{
	return 0;
}

int vfprintf(int f, char *fmt, int ap)
{
	if (f < 1)
	{
		f = 1;
	}
	return write(f, fmt, pnut_strlen(fmt));
}

int vsprintf(char *s, char *fmt, int ap)
{
	if (s)
	{
		s[0] = 0;
	}
	return 0;
}

void perror(char *s)
{
}

char *strerror(int e)
{
	return "";
}

char *strdup(char *s)
{
	return s;
}

char *strncat(char *d, char *s, int n)
{
	return d;
}

char *memchr(char *s, int c, int n)
{
	return 0;
}

int strcasecmp(char *a, char *b)
{
	return strcmp(a, b);
}

int strncasecmp(char *a, char *b, int n)
{
	return strncmp(a, b, n);
}

int sigaction(int sig, int *act, int *old)
{
	return 0;
}

int signal(int sig, int fn)
{
	return 0;
}

int getpid(void)
{
	return 1;
}

int atexit(int fn)
{
	return 0;
}

int rand(void)
{
	return 0;
}

void srand(int s)
{
}

char *alloca(int n)
{
	return pnut_dummy;
}

int isprint(int c)
{
	return c >= 32 && c < 127;
}

int isspace(int c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

int isdigit(int c)
{
	return c >= '0' && c <= '9';
}

int isxdigit(int c)
{
	return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int isalpha(int c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int isalnum(int c)
{
	return isalpha(c) || isdigit(c);
}

int toupper(int c)
{
	if (c >= 'a' && c <= 'z')
	{
		return c - 'a' + 'A';
	}
	return c;
}

int tolower(int c)
{
	if (c >= 'A' && c <= 'Z')
	{
		return c - 'A' + 'a';
	}
	return c;
}

int errno;

void tcc_tool_cross(int s, int argv, int option)
{
	exit(1);
}
