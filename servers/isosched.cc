#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <unistd.h>

#include <deque>

#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "common.hh"
#include "protocol.hh"
#include "workload.hh"
#include "server_common.hh"

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static std::deque<int> reqs;

static void *worker_thread(void *arg)
{
	int fd, cpu = (long) arg;

	set_affinity(cpu);

	pthread_mutex_lock(&lock);
	while (1) {
		while (reqs.empty()) {
			pthread_cond_wait(&cond, &lock);
		}

		fd = reqs.back();
		reqs.pop_back();
		pthread_mutex_unlock(&lock);

		do_workload(fd);

		pthread_mutex_lock(&lock);
	}

	return NULL;
}

static void main_thread(int server_fd)
{
	int fd, ret, flag = 1;
	struct sockaddr_in addr;
	socklen_t addr_len = sizeof(struct sockaddr_in);

	while (1) {
		fd = accept(server_fd, (struct sockaddr *) &addr, &addr_len);
		if (fd < 0) {
			perror("accept()");
			panic("main_thread: accept failed");
		}

		ret = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
				 (char *) &flag, sizeof(int));
		if (ret == -1) {
			perror("setsockopt()");
			panic("main_thread: setsockopt");
		}

		pthread_mutex_lock(&lock);
		reqs.push_front(fd);
		pthread_mutex_unlock(&lock);
		pthread_cond_signal(&cond);
	}
}

int main(int argc, char *argv[])
{
	int server_fd, initial_cpu;

	workload_setup(1000);
	server_fd = setup_server(argc, argv);

	initial_cpu = create_worker_per_core(worker_thread, true);
	set_affinity(initial_cpu);

	main_thread(server_fd);
}
