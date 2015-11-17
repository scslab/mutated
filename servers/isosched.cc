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

/**
 * Does a few common tasks that we expect socket-based servers to
 * want to do. In particular, it:
 *
 *    - Parses the command line and binds/listens to the right address
 *      (TODO: parsing not actually implemented)
 *
 *    - Calibrates the workload
 */
static int setup_server(void)
{
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

/**
 * Services a workload for a file descriptor.
 */
static void do_workload(int fd)
{
	int ret;
	struct req_pkt req;
	struct resp_pkt resp;
	workload *w;

	w = workload_alloc();
	if (!w) {
		panic("do_workload: workload_alloc failed");
	}

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

int main(void)
{
	int server_fd, initial_cpu;

	workload_setup(1000);
	server_fd = setup_server();

	initial_cpu = create_worker_per_core(worker_thread, true);
	set_affinity(initial_cpu);

	main_thread(server_fd);
}
