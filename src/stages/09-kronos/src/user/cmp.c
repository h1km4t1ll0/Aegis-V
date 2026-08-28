void *fopen(const char *path, const char *mode);
unsigned long fread(void *p, unsigned long sz, unsigned long n, void *f);
int fclose(void *f);
long write(int fd, const void *buf, unsigned long n);
void *malloc(unsigned long n);
int fseek(void *f, long off, int whence);
long ftell(void *f);

static unsigned long rd64(unsigned char *p)
{
    return (unsigned long)p[0]
        | ((unsigned long)p[1] << 8)
        | ((unsigned long)p[2] << 16)
        | ((unsigned long)p[3] << 24)
        | ((unsigned long)p[4] << 32)
        | ((unsigned long)p[5] << 40)
        | ((unsigned long)p[6] << 48)
        | ((unsigned long)p[7] << 56);
}

int main(int argc, char **argv)
{
    void *a;
    void *b;
    unsigned char *pa;
    unsigned char *pb;
    long na;
    long nb;
    long i;
    unsigned long phoff_a;
    unsigned long phoff_b;
    unsigned long poff;
    unsigned long pfsz;
    unsigned long lim;

    if (argc < 3) {
        write(1, "cmp fail\n", 9);
        return 1;
    }
    a = fopen(argv[1], "r");
    b = fopen(argv[2], "r");
    if (!a || !b) {
        write(1, "cmp fail\n", 9);
        return 1;
    }
    fseek(a, 0, 2);
    na = ftell(a);
    fseek(a, 0, 0);
    fseek(b, 0, 2);
    nb = ftell(b);
    fseek(b, 0, 0);
    if (na < 120 || nb < 120) {
        write(1, "cmp fail sz\n", 12);
        return 1;
    }
    pa = malloc((unsigned long)na);
    pb = malloc((unsigned long)nb);
    if (!pa || !pb) {
        write(1, "cmp fail\n", 9);
        return 1;
    }
    fread(pa, 1, (unsigned long)na, a);
    fread(pb, 1, (unsigned long)nb, b);
    fclose(a);
    fclose(b);
    if (pa[0] != 0x7f || pa[1] != 'E' || pb[0] != 0x7f || pb[1] != 'E') {
        write(1, "cmp fail\n", 9);
        return 1;
    }
    phoff_a = rd64(pa + 32);
    phoff_b = rd64(pb + 32);
    if (phoff_a != phoff_b || phoff_a + 56 > (unsigned long)na
        || phoff_b + 56 > (unsigned long)nb) {
        write(1, "cmp fail sz\n", 12);
        return 1;
    }
    poff = rd64(pa + (long)phoff_a + 8);
    pfsz = rd64(pa + (long)phoff_a + 32);
    if (poff != rd64(pb + (long)phoff_b + 8)
        || pfsz != rd64(pb + (long)phoff_b + 32)) {
        write(1, "cmp fail sz\n", 12);
        return 1;
    }
    lim = poff + pfsz;
    if (lim > (unsigned long)na || lim > (unsigned long)nb || lim < 120) {
        write(1, "cmp fail sz\n", 12);
        return 1;
    }
    i = 0;
    while ((unsigned long)i < lim) {
        if (pa[i] != pb[i]) {
            write(1, "cmp fail off\n", 13);
            return 1;
        }
        i = i + 1;
    }
    write(1, "cmp ok\n", 7);
    return 0;
}
