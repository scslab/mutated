#ifndef LIBSERVER_LINUX_COMPAT_HH
#define LIBSERVER_LINUX_COMPAT_HH

#include <pthread.h>

#include "debug.hh"

#ifdef __linux__

int set_affinity(int core);

#else /* !linux */

// XXX: Implement!
#define CLOCK_MONOTONIC 0

static inline int clock_gettime(int UNUSED clock, struct timespec*)
{
    // XXX: Implement!
    return 0;
}

int set_affinity(int core);

struct cpu_set_t {};

#define CPU_ISSET(i, cpuset) (true)

static inline int pthread_getaffinity_np(pthread_t, size_t, cpu_set_t*)
{
    return 0;
}

#endif /* linux or !linux*/

#endif /* LIBSERVER_LINUX_COMPAT_HH */
