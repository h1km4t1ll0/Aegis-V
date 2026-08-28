/* libc extras M2 bootstrap does not provide. Linked into PNUT.c. */

void pnut_alloc_bss();

int isatty(int fd)
{
	return 0;
}

void putchar(char c)
{
	fputc(c, stdout);
}

void print_int(long n)
{
	char buf[32];
	int i;
	unsigned long u;
	if(n < 0)
	{
		fputc('-', stdout);
		u = 0 - n;
	}
	else
	{
		u = n;
	}
	if(u == 0)
	{
		fputc('0', stdout);
		return;
	}
	i = 0;
	while(u > 0)
	{
		buf[i] = '0' + (u % 10);
		i = i + 1;
		u = u / 10;
	}
	while(i > 0)
	{
		i = i - 1;
		fputc(buf[i], stdout);
	}
}

void print_hex(unsigned long n)
{
	char buf[16];
	int i;
	unsigned long d;
	if(n == 0)
	{
		fputc('0', stdout);
		return;
	}
	i = 0;
	while(n > 0)
	{
		d = n & 15;
		if(d < 10)
		{
			buf[i] = '0' + d;
		}
		else
		{
			buf[i] = 'a' + (d - 10);
		}
		i = i + 1;
		n = n / 16;
	}
	while(i > 0)
	{
		i = i - 1;
		fputc(buf[i], stdout);
	}
}

/* Extra unused args are fine on RV64 (caller leaves them in a-regs). */
int printf(char* fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8)
{
	long args[8];
	int ai;
	char* p;
	args[0] = a1;
	args[1] = a2;
	args[2] = a3;
	args[3] = a4;
	args[4] = a5;
	args[5] = a6;
	args[6] = a7;
	args[7] = a8;
	ai = 0;
	p = fmt;
	while(p[0] != 0)
	{
		if(p[0] != '%')
		{
			fputc(p[0], stdout);
			p = p + 1;
			continue;
		}
		p = p + 1;
		if(p[0] == '%')
		{
			fputc('%', stdout);
			p = p + 1;
			continue;
		}
		if(p[0] == 's')
		{
			if(args[ai] != 0)
			{
				fputs((char*)args[ai], stdout);
			}
			ai = ai + 1;
			p = p + 1;
			continue;
		}
		if(p[0] == 'd' || p[0] == 'i')
		{
			print_int(args[ai]);
			ai = ai + 1;
			p = p + 1;
			continue;
		}
		if(p[0] == 'x')
		{
			print_hex(args[ai]);
			ai = ai + 1;
			p = p + 1;
			continue;
		}
		if(p[0] == 'c')
		{
			fputc(args[ai], stdout);
			ai = ai + 1;
			p = p + 1;
			continue;
		}
		fputc('%', stdout);
		fputc(p[0], stdout);
		p = p + 1;
	}
	return 0;
}

char* strrchr(char* str, int c)
{
	char* last;
	last = 0;
	while(str[0] != 0)
	{
		if(str[0] == c)
		{
			last = str;
		}
		str = str + 1;
	}
	return last;
}
