#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>

#include "client.hh"
#include "socket.hh"
#include "time.hh"

Client * client_;

/* Create a new client */
Client::Client(int argc, char *argv[])
	: cfg{argc, argv}, gen{new generator{}}, rd{}, randgen{rd()}
	, service_samples{}, wait_samples{} , throughput{0}
	, start_ts{}, in_count{0}, out_count{0}, measure_count{0}
	, step_pos{0}, step_count{0}
	, epollfd{-1}, timerfd{-1}
	, deadlines{(struct timespec *) malloc(sizeof(struct timespec) * cfg.total_samples)}
	, start_time{}
{
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

	if (gen->start()) {
		perror("failed to start generator");
		exit(1);
	}
}

/* Destructor */
Client::~Client(void)
{
}

#define MAX_EVENTS	1000

void Client::run(void)
{
	int nfds, i;
	struct epoll_event events[MAX_EVENTS];

	setup_experiment();

	while (true) {
		nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);

		for (i = 0; i < nfds; i++) {
			struct epoll_event *ev = &events[i];

			/* is it a timer? */
			if (!ev->data.ptr) {
				timer_handler();
			} else {
				Sock *s = (Sock *) ev->data.ptr;
				s->handler(ev->events);
			}
		}
	}
}

/**
 * epoll_watch - registers a file descriptor for epoll events
 * @fd: the file descriptor
 * @data: a cookie for the event
 * @event: the event mask
 *
 * Returns 0 if successful, otherwise < 0.
 */
int Client::epoll_watch(int fd, void *data, uint32_t events)
{
	struct epoll_event ev;
	ev.events = events | EPOLLET;
	ev.data.ptr = data;
	return epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
}

void Client::setup_experiment(void)
{
	step_pos += cfg.step_size;
	step_count++;

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

void Client::setup_deadlines(void)
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

void Client::do_request(void)
{
	bool should_measure;

	if (in_count == cfg.pre_samples + 1) {
		if (clock_gettime(CLOCK_MONOTONIC, &start_ts)) {
			perror("clock_gettime()");
			exit(1);
		}
	}

	should_measure = in_count > cfg.pre_samples
		and in_count <= cfg.pre_samples + cfg.samples;

	if (gen->do_request(should_measure)) {
		// panic("generator failed to make request");
		exit(1);
	}
}

/**
 * Set timer to fire in usec time *since the last timer firing*
 * (as recorded by last_time).  This means if you call timer_arm()
 * with a sequence of usecs based on some probability distribution,
 * the timer will trigger will according to that distribution,
 * and not the distribution + processing time.
 */
int Client::timer_arm(struct timespec deadline)
{
	struct itimerspec itval;
	itval.it_interval.tv_sec = 0;
	itval.it_interval.tv_nsec = 0;
	itval.it_value = deadline;
	return timerfd_settime(timerfd, 0, &itval, NULL);
}

void Client::timer_handler(void)
{
	struct timespec now_time, result, sleep_time;

	if (clock_gettime(CLOCK_MONOTONIC, &now_time)) {
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

	if (timer_arm(sleep_time) < 0) {
		perror("timer_arm()");
		exit(1);
	}
}

/* Record a latency sample */
void Client::record_sample(uint64_t service_us, uint64_t wait_us, bool should_measure)
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
			perror("read_completion_handler: clock_gettime()");
			exit(1);
		}

		if (timespec_subtract(&now, &start_ts, &delta)) {
			fprintf(stderr, "experiment finished before it started");
			exit(1);
		}
		delta_us = timespec_to_us(&delta);
		throughput = (double) cfg.samples / ((double) delta_us / (double) USEC);
	}

	out_count++;
	if (out_count >= cfg.total_samples) {
		print_summary();
		setup_experiment();
	}
}

/* Print summary of current results */
void Client::print_summary(void)
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
