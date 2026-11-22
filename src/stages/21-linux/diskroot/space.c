#include <sys/statvfs.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    struct statvfs s;
    if (argc != 2 || statvfs(argv[1], &s)) return 1;
    unsigned long long bytes = (unsigned long long)s.f_bavail * s.f_frsize;
    printf("diskroot install free bytes %llu\n", bytes);
    return bytes < 350ULL * 1024 * 1024;
}
