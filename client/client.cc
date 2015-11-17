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

#include "client.hh"
#include "common.hh"
#include "protocol.hh"
#include "time.hh"
#include "accum.hh"
#include "socket.hh"
#include "generator.hh"

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
static uint64_t total_samples;

static struct config cfg;

static int epollfd;
static int timerfd;

static void setup_experiment(void);

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
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
		perror("epoll_ctl()");
		return -1;
	}

	return 0;
}

static struct timespec *deadlines;
static struct timespec start_time;

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
#if 0
	fprintf(stderr, "throughput: %f\n", throughput);
	fprintf(stderr, "service: min %ld, mean %f, stddev %f, 99th %ld, max %ld\n",
	       service_samples.min(), service_samples.mean(),
	       service_samples.stddev(),
	       service_samples.percentile(0.99),
	       service_samples.max());

	fprintf(stderr, "wait: min %ld, mean %f, stddev %f, 99th %ld, max %ld\n",
	       wait_samples.min(), wait_samples.mean(),
	       wait_samples.stddev(),
	       wait_samples.percentile(0.99),
	       wait_samples.max());
#endif

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

#if 0
static int lb_cnt[1024];

static void socket_lb_create(struct sock *s)
{
	int idx, ret;
	static int lb_rr;

	if (cfg.least_loaded) {
		int i, min = lb_cnt[0];
		idx = 0;

		for (i = 1; i < cfg.lb_cnt; i++) {
			if (lb_cnt[i] < min) {
				min = lb_cnt[i];
				idx = i;
			}
		}
	} else {
		idx = (lb_rr++ % cfg.lb_cnt);
	}

	idx = rand() % cfg.lb_cnt;

	lb_cnt[idx]++;
	ret = socket_create(s, cfg.addr, cfg.port + idx);
	if (ret)
		panic("timer_handler: socket_create() failed");
}
#endif

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
	if (out_count >= cfg.pre_samples + cfg.samples + cfg.post_samples) {
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
	//	printf("%ld %ld %ld %ld\n", deadlines[in_count].tv_sec, deadlines[in_count].tv_nsec, result.tv_sec, result.tv_nsec);
		do_request();
		in_count++;
		if (in_count > total_samples)
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
	unsigned int i;
	double accum = 0;

	// Exponential distribution suggested by experimental evidence,
	// c.f. Figure 11 in "Power Management of Online Data-Intensive Services"
	std::exponential_distribution<double> d(1 / (double) cfg.arrival_us);

	for (i = 0; i < total_samples; i++) {
		accum += d(randgen);
		us_to_timespec(ceil(accum), &deadlines[i]);
	}
}

static void setup_experiment(void)
{
	int ret;

	step_pos += cfg.step_size;
	step_count++;
	// Finished running all the experiments?
	if (step_pos > cfg.step_stop + 10) {
		exit(0);
	}
	cfg.arrival_us = (double) USEC / step_pos;
	in_count = out_count = measure_count = 0;

	setup_deadlines();

	ret = clock_gettime(CLOCK_MONOTONIC, &start_time);
	if (ret == -1) {
		perror("clock_gettime()");
		exit(1);
	}

	timer_handler();
}

/**
 * cpu_pin - pin down the local thread to a core
 * @cpu: the target core
 *
 * Returns 0 if successful, otherwise < 0.
 */
static int cpu_pin(unsigned int cpu)
{
	int ret;
	cpu_set_t mask;

	CPU_ZERO(&mask);
	CPU_SET(cpu, &mask);

	ret = sched_setaffinity(0, sizeof(mask), &mask);
	if (ret)
		return ret;

	return 0;
}

static void parse_args(int argc, char *argv[])
{
	int ret, workers, steps;

	// default config
	cfg.pre_samples = 100;
	cfg.samples = 1000;
	cfg.post_samples = 100;
	cfg.label = "default";
	cfg.lb_cnt = 1;

	opterr = 0;
	int c;
	while ((c = getopt(argc, argv, "hbmw:s:c:l:n:")) != -1) {
		switch (c) {
		case 'h':
			goto help;
		case 'm':
			cfg.machine_readable = true;
			break;
		case 'w':
			cfg.pre_samples = atoi(optarg);
			break;
		case 's':
			cfg.samples = atoi(optarg);
			break;
		case 'c':
			cfg.post_samples = atoi(optarg);
			break;
		case 'n':
			cfg.lb_cnt = atoi(optarg);
			break;
		case 'b':
			cfg.least_loaded = true;
			break;
		case 'l':
			cfg.label = optarg;
			break;
		default:
			goto fail;
		}
	}

	if (argc - optind != 4)
		goto fail;

	ret = sscanf(argv[optind+0], "%[^:]:%hu", cfg.addr, &cfg.port);
	if (ret != 2)
		goto fail;
	ret = sscanf(argv[optind+1], "%d", &workers);
	if (ret != 1)
		goto fail;
	if (workers <= 0)
		goto fail;
	ret = sscanf(argv[optind+2], "%d", &steps);
	if (ret != 1)
		goto fail;
	if (steps <= 0)
		goto fail;
	ret = sscanf(argv[optind+3], "%lf", &cfg.service_us);
	if (ret != 1)
		goto fail;

	cfg.step_size = (double) USEC / cfg.service_us / steps * workers;
	cfg.step_stop = (double) ceil(USEC / cfg.service_us * workers);

	total_samples = cfg.pre_samples + cfg.samples + cfg.post_samples;
	deadlines = (struct timespec *) malloc(sizeof(struct timespec) * total_samples);
	if (!deadlines)
		exit(1);

	return;

fail:
	fprintf(stderr, "invalid arguments\n");
help:
	fprintf(stderr, "usage: %s [-h] [-m] [-w integer] [-s integer] [-c integer] ip:port workers steps service_mean_us\n", argv[0]);
	fprintf(stderr, "  -h: help\n");
	fprintf(stderr, "  -m: machine-readable\n");
	fprintf(stderr, "  -w: warm-up sample count\n");
	fprintf(stderr, "  -s: measurement sample count\n");
	fprintf(stderr, "  -c: cool-down sample count\n");
	fprintf(stderr, "  -l: label for machine-readable output (-m)\n");

	exit(1);
}

int main(int argc, char *argv[])
{
	int ret;

	parse_args(argc, argv);

	ret = cpu_pin(0);
	if (ret)
		return ret;

	epollfd = epoll_create1(0);
	if (epollfd == -1) {
		perror("epoll_create()");
		exit(1);
	}

	timerfd = timerfd_create(CLOCK_MONOTONIC, O_NONBLOCK);
	if (timerfd == -1) {
		perror("timerfd_create()");
		exit(1);
	}

	ret = epoll_watch(timerfd, NULL, EPOLLIN);
	if (ret == -1) {
		exit(1);
	}

	// ToDo: also support *detailed* sample information
	// NB: Keep this up to date with print_summary()
	if (cfg.machine_readable) {
		printf("label\tservice_us\tarrival_us\tstep_count\trequests_per_sec\tideal_requests_per_sec\tservice_min\tservice_mean\tservice_stddev\tservice_99th\tservice_99.9th\tservice_max\twait_min\twait_mean\twait_stddev\twait_99th\twait_99.9th\twait_max\n");
	} else {
		printf("#reqs/s\t\t(ideal)\t\tmin\tavg\t\tstd\t\t99th\t99.9th\tmax\tmin\tavg\t\tstd\t\t99th\t99.9th\tmax\n");
	}

	gen = new generator_flowperreq(cfg);
	if (!gen)
		exit(1);

	ret = gen->start();
	if (ret) {
		perror("failed to start generator");
		exit(1);
	}

	setup_experiment();
	main_loop();
}
