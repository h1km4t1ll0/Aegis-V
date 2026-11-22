/* GNU sed 4.8 configure's rmdir test, with progress around each call. */
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void)
{
    int result = 0;
    int rc, error;
    FILE *file;

    setvbuf(stdout, NULL, _IONBF, 0);
    if (mkdir("conftest.dir", 0755) != 0)
        return 8;
    file = fopen("conftest.file", "w");
    if (!file || fclose(file) != 0)
        return 8;

    puts("rmdir file/ begin");
    errno = 0;
    rc = rmdir("conftest.file/");
    error = errno;
    printf("rmdir file/ returned %d errno %d\n", rc, error);
    if (!rc)
        result |= 1;
    else if (error != ENOTDIR)
        result |= 2;

    puts("rmdir dir/./ begin");
    errno = 0;
    rc = rmdir("conftest.dir/./");
    error = errno;
    printf("rmdir dir/./ returned %d errno %d\n", rc, error);
    if (!rc)
        result |= 4;

    printf("rmdir probe result %d\n", result);
    if (result)
        return result;
    if (unlink("conftest.file") || rmdir("conftest.dir"))
        return 8;
    puts("rmdir probe ok");
    return 0;
}
