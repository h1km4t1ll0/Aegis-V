#ifndef SYS_MEMBARRIER_H
#define SYS_MEMBARRIER_H

#include_next <sys/membarrier.h>
#include <features.h>

hidden int __membarrier(int, int);

#endif
