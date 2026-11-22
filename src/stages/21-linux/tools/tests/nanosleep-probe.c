/* sed's huge-nanosleep configure test needs alarm(), hence setitimer(). */
#include <errno.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

int main(void)
{
    struct itimerval timer = {0};
    struct timespec delay = {0, 1};
    int rc, error;

    setvbuf(stdout, NULL, _IONBF, 0);
    timer.it_value.tv_sec = 1;
    errno = 0;
    rc = setitimer(ITIMER_REAL, &timer, NULL);
    error = errno;
    printf("setitimer returned %d errno %d\n", rc, error);
    if (rc != -1 || error != ENOSYS)
        return 1;

    errno = 0;
    rc = nanosleep(&delay, NULL);
    error = errno;
    printf("short nanosleep returned %d errno %d\n", rc, error);
    if (rc != 0)
        return 1;
    puts("nanosleep compatibility probe ok");
    return 0;
}
