long ecall3(long n, long a, long b, long c);

static void w1(const char *s, long n)
{
    ecall3(64, 1, (long)s, n);
}

static void putnum(unsigned long v)
{
    char d[20];
    int i;
    i = 0;
    if (v == 0) {
        d[0] = '0';
        w1(d, 1);
        return;
    }
    while (v != 0) {
        d[i] = (char)('0' + (v % 10));
        i = i + 1;
        v = v / 10;
    }
    while (i > 0) {
        i = i - 1;
        w1(d + i, 1);
    }
}

void go(void)
{
    char path[16];
    char pref[8];
    char buf[4096];
    char nlc;
    long fd;
    long n;
    unsigned long total;

    path[0] = '/';
    path[1] = 's';
    path[2] = 'r';
    path[3] = 'c';
    path[4] = '/';
    path[5] = 't';
    path[6] = 'c';
    path[7] = 'c';
    path[8] = '/';
    path[9] = 't';
    path[10] = 'c';
    path[11] = 'c';
    path[12] = '.';
    path[13] = 'c';
    path[14] = 0;

    pref[0] = 't';
    pref[1] = 'c';
    pref[2] = 'c';
    pref[3] = '.';
    pref[4] = 'c';
    pref[5] = ' ';

    nlc = 10;

    fd = ecall3(56, -100, (long)path, 0);
    total = 0;
    for (;;) {
        n = ecall3(63, fd, (long)buf, 4096);
        if (n <= 0)
            break;
        total = total + (unsigned long)n;
    }
    ecall3(57, fd, 0, 0);
    w1(pref, 6);
    putnum(total);
    w1(&nlc, 1);
    ecall3(93, 0, 0, 0);
}
