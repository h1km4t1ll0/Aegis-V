#define _GNU_SOURCE
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/sysmacros.h>
#include <stdio.h>
#include <unistd.h>

int main(void)
{
    struct statfs fs;
    struct stat root, block;
    if (statfs("/", &fs) || stat("/", &root) || stat("/dev/vda", &block)) {
        perror("root stat"); return 1;
    }
    if (fs.f_type != 0xef53 || root.st_dev != block.st_rdev || !S_ISBLK(block.st_mode)) {
        fputs("root must be ext4 on /dev/vda\n", stderr); return 1;
    }
    printf("diskroot root ext4 dev=%u:%u free=%llu\n", major(root.st_dev), minor(root.st_dev),
           (unsigned long long)fs.f_bavail * fs.f_bsize);
    return 0;
}
