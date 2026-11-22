#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    int fd;
    if (argc != 2) return 2;
    fd = open(argv[1], O_RDWR);
    if (fd < 0) { perror("open export"); return 1; }
    if (fsync(fd)) { perror("fsync export"); close(fd); return 1; }
    if (close(fd)) { perror("close export"); return 1; }
    sync();
    puts("selfhost export fsync ok");
    return 0;
}
