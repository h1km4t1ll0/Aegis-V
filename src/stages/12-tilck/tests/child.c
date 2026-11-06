/* Exec target of tests/proc.c under Tilck: prints its argv and exits 0
   (1 if the argv proc passed did not arrive). */

long write(int fd, const void *buf, unsigned long n);

static void say(const char *s)
{
    unsigned long n;

    n = 0;
    while (s[n])
        n = n + 1;
    write(1, s, n);
}

int main(int argc, char **argv)
{
    say("child: ");
    say(argv[0]);
    if (argc != 2 || argv[1][0] != 'a' || argv[1][3] != '1') {
        say(" bad argv\n");
        return 1;
    }
    say(" ");
    say(argv[1]);
    say("\n");
    return 0;
}
