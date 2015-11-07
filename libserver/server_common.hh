#ifndef SERVER_COMMON_HH
#define SERVER_COMMON_HH

int setup_server(int argc, char *argv[]);

int create_worker_per_core(void *(*worker_thread) (void *), bool reserve_cpu);

int set_affinity(int core);

void do_workload(int fd);

#endif /* SERVER_COMMON_HH */
