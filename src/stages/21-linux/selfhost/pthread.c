#define _GNU_SOURCE
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sched.h>
#include <stdio.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_barrier_t gate;
static long tids[2], pids[2], counter;

static void *worker(void *arg)
{
    long n = (long)arg;
    int r;
    tids[n] = syscall(SYS_gettid);
    pids[n] = getpid();
    r = pthread_barrier_wait(&gate);
    if (r && r != PTHREAD_BARRIER_SERIAL_THREAD) return (void *)1;
    for (int i = 0; i < 10000; i++) {
        if (pthread_mutex_lock(&lock)) return (void *)2;
        counter++;
        if (pthread_mutex_unlock(&lock)) return (void *)3;
        if (!(i % 64)) sched_yield();
    }
    return 0;
}

int main(void)
{
    pthread_t a, b;
    void *ra, *rb;
    int r;
    if (pthread_barrier_init(&gate, 0, 3)) return 1;
    if (pthread_create(&a, 0, worker, (void *)0) ||
        pthread_create(&b, 0, worker, (void *)1)) return 2;
    r = pthread_barrier_wait(&gate);
    if (r && r != PTHREAD_BARRIER_SERIAL_THREAD) return 3;
    if (pthread_join(a, &ra) || pthread_join(b, &rb) || ra || rb) return 4;
    if (counter != 20000 || tids[0] == tids[1] ||
        pids[0] != getpid() || pids[1] != getpid()) return 5;
    printf("linux pthread ok pid=%ld tids=%ld,%ld counter=%ld\n",
           (long)getpid(), tids[0], tids[1], counter);
    return 0;
}
