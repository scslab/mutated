#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "common.hh"
#include "protocol.hh"
#include "server_common.hh"
#include "workload.hh"

/**
 * Does a few common tasks that we expect socket-based servers to
 * want to do.  In particular, it:
 *
 *    - Parses the command line and binds/listens to the right address
 *      (TODO: parsing not actually implemented)
 *
 *    - Calibrates the workload
 */
int setup_server(int argc, char *argv[]) {
	struct sockaddr_in s_in;
	int server_fd, ret, flag = 1;

	server_fd = socket(PF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
		perror("socket()");
		exit(1);
	}

	ret = setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
			 (char *) &flag, sizeof(int));
	if (ret) {
		perror("unable to set reusable address\n");
	}

	memset(&s_in, 0, sizeof(s_in));
	s_in.sin_family         = PF_INET;
	s_in.sin_addr.s_addr    = INADDR_ANY;
	s_in.sin_port           = htons(8080);

	ret = bind(server_fd, (struct sockaddr*) &s_in, sizeof(s_in));
	if (ret == -1) {
		perror("bind()");
		exit(1);
	}

	ret = listen(server_fd, SOMAXCONN);
	if (ret) {
		perror("listen()");
		exit(1);
	}

	return server_fd;
}

static int get_num_cpus(void)
{
	return sysconf(_SC_NPROCESSORS_CONF);
}

int set_affinity(int core)
{
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(core, &cpuset);

	return pthread_setaffinity_np(pthread_self(),
		sizeof(cpu_set_t), &cpuset);
}

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
int create_worker_per_core(void *(*worker_thread) (void *), bool reserve_cpu) {
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
		if (!CPU_ISSET(i, &cpuset)) continue;
		if (initial_cpu < 0) {
			initial_cpu = i;
			continue;
		}
		ret = pthread_create(&tid, NULL, worker_thread,
			(void *) (long) i);
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
		worker_thread((void*) (long) initial_cpu);
	}

	return initial_cpu;
}

/**
 * Services a workload for a file descriptor.
 */
void do_workload(int fd)
{
	int ret;
	struct req_pkt req;
	struct resp_pkt resp;
	struct workload *w;

	w = workload_alloc();
	if (!w)
		panic("do_workload: workload_alloc failed");

	ret = read(fd, (void *) &req, sizeof(req));
	if (ret != sizeof(req)) {
		// this could happen if the load generator terminates
		goto out;
	}

	workload_run(w, req.delays[0]);

	resp.tag = req.tag;
	ret = write(fd, (void *) &resp, sizeof(resp));
	if (ret != sizeof(resp)) {
		// this could happen if the load generator terminates
		goto out;
	}

out:
	free(w);
	close(fd);
}
