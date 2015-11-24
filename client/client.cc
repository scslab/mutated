#include <random>

#include <errno.h>
#include <sched.h>
#include <stdint.h>
#include <time.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

#include "accum.hh"
#include "client.hh"
#include "debug.hh"
#include "generator.hh"
#include "opts.hh"
#include "protocol.hh"
#include "server_common.hh"
#include "socket.hh"
#include "time.hh"

static generator *gen;
static std::random_device rd;
std::mt19937 randgen(rd());

static accum service_samples;
static accum wait_samples;
static double throughput;

static struct timespec start_ts;
static uint64_t in_count, out_count, measure_count;
/* requests per second (the desired request rate) */
static double step_pos = 0;
/* step_count = step_pos / step_size */
static uint64_t step_count = 0;

static struct Config cfg;

static int epollfd;
static int timerfd;

static void setup_experiment(void);

static struct timespec *deadlines;
static struct timespec start_time;

/**
 * epoll_watch - registers a file descriptor for epoll events
 * @fd: the file descriptor
 * @data: a cookie for the event
 * @event: the event mask
 *
 * Returns 0 if successful, otherwise < 0.
 */
int epoll_watch(int fd, void *data, uint32_t events)
{
	struct epoll_event ev;
	ev.events = events | EPOLLET;
	ev.data.ptr = data;
	return epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
}

/**
 * Set timer to fire in usec time *since the last timer firing*
 * (as recorded by last_time).  This means if you call timer_arm()
 * with a sequence of usecs based on some probability distribution,
 * the timer will trigger will according to that distribution,
 * and not the distribution + processing time.
 */
static int timer_arm(struct timespec deadline)
{
	int ret;
	struct itimerspec itval;

	itval.it_interval.tv_sec = 0;
	itval.it_interval.tv_nsec = 0;
	itval.it_value = deadline;

	ret = timerfd_settime(timerfd, 0, &itval, NULL);
	if (ret == -1) {
		perror("timerfd_settime()");
		return -errno;
	}

	return 0;
}

static void print_summary(void)
{
	// NB: Keep this up to date with main()
	if (cfg.machine_readable) {
		printf("%s\t%f\t%f\t%lu\t", // no newline
					 cfg.label,
					 cfg.service_us,
					 cfg.arrival_us,
					 step_count);
	}

	printf("%f\t%f\t%ld\t%f\t%f\t%ld\t%ld\t%ld\t%ld\t%f\t%f\t%ld\t%ld\t%ld\n",
				 throughput,
				 step_pos,
				 service_samples.min(),
				 service_samples.mean(),
				 service_samples.stddev(),
				 service_samples.percentile(0.99),
               service_samples.percentile(0.999),
				 service_samples.max(),
				 wait_samples.min(),
				 wait_samples.mean(),
				 wait_samples.stddev(),
				 wait_samples.percentile(0.99),
				 wait_samples.percentile(0.999),
				 wait_samples.max());
}

void record_sample(uint64_t service_us, uint64_t wait_us, bool should_measure)
{
	if (should_measure) {
		measure_count++;
		service_samples.add_sample(service_us);
		wait_samples.add_sample(wait_us);
	}

	if (measure_count == cfg.samples) {
		struct timespec now, delta;
		uint64_t delta_us;

		measure_count++;

		if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) {
			panic("read_completion_handler: clock_gettime()");
		}

		if (timespec_subtract(&now, &start_ts, &delta)) {
			panic("experiment finished before it started");
		}
		delta_us = timespec_to_us(&delta);
		throughput = (double) cfg.samples /
					 ((double) delta_us / (double) USEC);
	}

	out_count++;
	if (out_count >= cfg.total_samples) {
		print_summary();
		setup_experiment();
	}
}

static void do_request(void)
{
	bool should_measure;
	int ret;

	if (in_count == cfg.pre_samples + 1) {
		ret = clock_gettime(CLOCK_MONOTONIC, &start_ts);
		if (ret == -1) {
			perror("clock_gettime()");
			panic("clock_gettime");
		}
	}

	if (in_count > cfg.pre_samples &&
			in_count <= cfg.pre_samples + cfg.samples)
		should_measure = true;
	else
		should_measure = false;

	if (gen->do_request(should_measure)) {
		panic("generator failed to make request");
	}
}

static void timer_handler(void)
{
	int ret;
	struct timespec now_time, result, sleep_time;

	ret = clock_gettime(CLOCK_MONOTONIC, &now_time);
	if (ret == -1) {
		perror("clock_gettime()");
		exit(1);
	}

	timespec_subtract(&now_time, &start_time, &result);

	while(timespec_subtract(&deadlines[in_count], &result, &sleep_time)) {
		do_request();
		in_count++;
		if (in_count > cfg.total_samples)
			return;
	}

	ret = timer_arm(sleep_time);
	if (ret)
		panic("timer_handler: timer_arm() failed");
}

#define MAX_EVENTS	1000

static void main_loop(void)
{
	int nfds, i;
	struct epoll_event events[MAX_EVENTS];

	while (1) {
		nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);

		for (i = 0; i < nfds; i++) {
			struct epoll_event *ev = &events[i];

			/* is it a timer? */
			if (!ev->data.ptr) {
				timer_handler();
			} else {
				socket_handler((struct sock *) ev->data.ptr, ev->events);
			}
		}
	}
}

static void setup_deadlines(void)
{
	// Exponential distribution suggested by experimental evidence,
	// c.f. Figure 11 in "Power Management of Online Data-Intensive Services"
	std::exponential_distribution<double> d(1 / (double) cfg.arrival_us);

	double accum = 0;
	for (unsigned int i = 0; i < cfg.total_samples; i++) {
		accum += d(randgen);
		us_to_timespec(ceil(accum), &deadlines[i]);
	}
}

static void setup_experiment(void)
{
	step_pos += cfg.step_size;
	step_count++;
  //
	// Finished running all the experiments?
	if (step_pos > cfg.step_stop + 10) {
		exit(0);
	}

	cfg.arrival_us = (double) USEC / step_pos;
	in_count = out_count = measure_count = 0;

	setup_deadlines();

	if (clock_gettime(CLOCK_MONOTONIC, &start_time)) {
		perror("clock_gettime()");
		exit(1);
	}

	timer_handler();
}

int old_main(int argc, char *argv[])
{
	cfg = Config(argc, argv);

	if (set_affinity(0)) {
    perror("set_affinity()");
    exit(1);
  }

	if ((epollfd = epoll_create1(0)) < 0) {
		perror("epoll_create()");
		exit(1);
	}

	if ((timerfd = timerfd_create(CLOCK_MONOTONIC, O_NONBLOCK)) < 0) {
		perror("timerfd_create()");
		exit(1);
	}

	if (epoll_watch(timerfd, NULL, EPOLLIN) < 0) {
    perror("epoll_watch");
    exit(1);
  }

	// TODO: also support detailed sample information
	// NB: Keep this up to date with print_summary()
	if (cfg.machine_readable) {
		printf("label\tservice_us\tarrival_us\tstep_count\trequests_per_sec"
      "\tideal_requests_per_sec\tservice_min\tservice_mean\tservice_stddev"
      "\tservice_99th\tservice_99.9th\tservice_max\twait_min\twait_mean"
      "\twait_stddev\twait_99th\twait_99.9th\twait_max\n");
	} else {
		printf("#reqs/s\t\t(ideal)\t\tmin\tavg\t\tstd\t\t99th\t99.9th"
      "\tmax\tmin\tavg\t\tstd\t\t99th\t99.9th\tmax\n");
	}

	gen = new generator_flowperreq(cfg);
	if (gen->start()) {
		perror("failed to start generator");
		exit(1);
	}

	setup_experiment();
	main_loop();
	return EXIT_SUCCESS;
}
