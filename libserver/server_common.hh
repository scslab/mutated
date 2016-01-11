#ifndef SERVER_COMMON_HH
#define SERVER_COMMON_HH

int create_worker_per_core(void *(*worker_thread)(void *), bool reserve_cpu);

int set_affinity(int core);

#endif /* SERVER_COMMON_HH */
