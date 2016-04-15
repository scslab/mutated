#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "debug.hh"
#include "linux_compat.hh"
#include "protocol.hh"
#include "server_common.hh"
#include "workload.hh"

static int get_num_cpus(void) { return sysconf(_SC_NPROCESSORS_CONF); }

/**
 * Spawns N pthreads, where N is the number of CPU cores available
 * to this process according to its current affinity (i.e. set by
 * taskset.)  This models the situation where you want to create a
 * worker per core.
 *
 * If reserve_cpu is true, returns the number of the CPU which was
 * reserved (e.g. did not have a worker created for it).  Use this
 * if you need some sort of main, dispatch thread.
 */
int create_worker_per_core(void *(*worker_thread)(void *), bool reserve_cpu)
{
    int nr_cpus, ret;
    pthread_t tid;

    nr_cpus = get_num_cpus();
    if (nr_cpus < 2) {
        panic("failed to get CPU count");
    }

    /* read out current affinity (from parent taskset) so we respect it*/
    cpu_set_t cpuset;
    ret = pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (ret) {
        perror("pthread_getaffinity_np()");
        exit(1);
    }

    int initial_cpu = -1; // fake value
    bool started_worker = false;
    for (int i = 0; i < nr_cpus; i++) {
        // ignore CPUs which were not in our initial mask
        if (!CPU_ISSET(i, &cpuset))
            continue;
        if (initial_cpu < 0) {
            initial_cpu = i;
            continue;
        }
        ret = pthread_create(&tid, NULL, worker_thread, (void *)(long)i);
        if (ret == -1) {
            perror("thread_create()");
            exit(1);
        }
        started_worker = true;
    }

    if (initial_cpu < 0) {
        panic("Could not allocate CPU for main thread");
    }

    if (!started_worker && !reserve_cpu) {
        panic("Could not find any usable CPUs for worker threads");
    }

    if (!reserve_cpu) {
        worker_thread((void *)(long)initial_cpu);
    }

    return initial_cpu;
}
