/* Open argv[1], print pnut-tcc-ok if it starts with ELF magic. */
void hexnibble(int v)
{
	char d[1];
	v = v & 15;
	if(v < 10)
	{
		d[0] = '0' + v;
	}
	else
	{
		d[0] = 'a' + (v - 10);
	}
	write(1, d, 1);
}

void hexbyte(int v)
{
	if(v < 0)
	{
		v = v + 256;
	}
	hexnibble(v >> 4);
	hexnibble(v);
}

int main(int argc, char** argv)
{
	char tmp[1];
	int n;
	int b0;
	int b1;
	int b2;
	int b3;
	FILE* f;
	if(argc < 2)
	{
		write(1, "usage\n", 6);
		return 1;
	}
	f = open(argv[1], 0, 0);
	if(0 > f)
	{
		write(1, "pnut-tcc-missing\n", 17);
		return 1;
	}
	write(1, "pnut-hex:", 9);
	n = 0;
	b0 = -1;
	b1 = -1;
	b2 = -1;
	b3 = -1;
	while(n < 16)
	{
		if(read(f, tmp, 1) != 1)
		{
			break;
		}
		hexbyte(tmp[0]);
		if(n == 0)
		{
			b0 = tmp[0];
		}
		if(n == 1)
		{
			b1 = tmp[0];
		}
		if(n == 2)
		{
			b2 = tmp[0];
		}
		if(n == 3)
		{
			b3 = tmp[0];
		}
		n = n + 1;
	}
	write(1, "\n", 1);
	if(n < 4)
	{
		write(1, "pnut-tcc-short\n", 15);
		return 1;
	}
	if(b0 < 0)
	{
		b0 = b0 + 256;
	}
	if(b1 < 0)
	{
		b1 = b1 + 256;
	}
	if(b2 < 0)
	{
		b2 = b2 + 256;
	}
	if(b3 < 0)
	{
		b3 = b3 + 256;
	}
	if(b0 == 127)
	{
		if(b1 == 69)
		{
			if(b2 == 76)
			{
				if(b3 == 70)
				{
					write(1, "pnut-tcc-ok\n", 12);
					return 0;
				}
			}
		}
	}
	b0 = b0 & 127;
	if(b0 == 19)
	{
		write(1, "pnut-tcc-ok\n", 12);
		return 0;
	}
	if(b0 == 23)
	{
		write(1, "pnut-tcc-ok\n", 12);
		return 0;
	}
	if(b0 == 111)
	{
		write(1, "pnut-tcc-ok\n", 12);
		return 0;
	}
	write(1, "pnut-tcc-not-elf\n", 17);
	return 1;
}
