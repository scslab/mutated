#include "linux_compat.hh"

#ifdef __linux__

int set_affinity(int core)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);

    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

#else /* !linux */

int set_affinity(int UNUSED core)
{
    // TODO: Implement
    return 0;
}

#endif /* linux or !linux */
