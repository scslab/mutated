#include <iostream>

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>

#include "client.hh"
#include "socket.hh"
#include "time.hh"
#include "util.hh"

using namespace std;

Client * client_;

/* Create a new client */
Client::Client(int argc, char *argv[])
	: cfg{argc, argv}, gen{new generator{}}, rd{}, randgen{rd()}
	, service_samples{}, wait_samples{} , throughput{0}
	, start_ts{}, in_count{0}, out_count{0}, measure_count{0}
	, step_pos{0}, step_count{0}
	, epollfd{-1}, timerfd{-1}
	, deadlines{(timespec *) malloc(sizeof(timespec) * cfg.total_samples)}
	, start_time{}
{
	epollfd = SystemCall(
		epoll_create1(0),
		"Client::Client: epoll_create1()");

	timerfd = SystemCall(
		timerfd_create(CLOCK_MONOTONIC, O_NONBLOCK),
		"Client::Client: timerfd_create()");

	epoll_watch(timerfd, NULL, EPOLLIN);


	print_header();
	gen->start();
}

/* Destructor */
Client::~Client(void)
{
	// TODO: Implement.
}

/**
 * Run a client, generate load and measure repsonse times.
 */
void Client::run(void)
{
	/* Maximum outstanding epoll events supported. */
	constexpr size_t MAX_EVENTS = 1000;
	epoll_event events[MAX_EVENTS];

	setup_experiment();

	while (true) {
		int nfds = SystemCall(
			epoll_wait(epollfd, events, MAX_EVENTS, -1),
			"Client::run: epoll_wait()");

		for (int i = 0; i < nfds; i++) {
			epoll_event &ev = events[i];

			/* is it a timer? */
			if (ev.data.ptr == nullptr) {
				timer_handler();
			} else {
				Sock *s = (Sock *) ev.data.ptr;
				s->handler(ev.events);
			}
		}
	}
}

/**
 * epoll_watch - registers a file descriptor for epoll events
 * @fd: the file descriptor
 * @data: a cookie for the event
 * @event: the event mask
 */
void Client::epoll_watch(int fd, void *data, uint32_t events)
{
	epoll_event ev;
	ev.events = events | EPOLLET;
	ev.data.ptr = data;
	SystemCall(
		epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev),
		"Client::epoll_watch: epoll_ctl()");
}

void Client::setup_experiment(void)
{
	step_pos += cfg.step_size;
	step_count++;

	// Finished running all the experiments?
	// TODO: Why + 10?
	if (step_pos > cfg.step_stop + 10) {
		exit(EXIT_SUCCESS);
	}

	cfg.arrival_us = (double) USEC / step_pos;
	in_count = out_count = measure_count = 0;

	setup_deadlines();

	SystemCall(
		clock_gettime(CLOCK_MONOTONIC, &start_time),
		"Client::setup_experiment: clock_gettime");

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
	if (in_count == cfg.pre_samples + 1) {
		SystemCall(
			clock_gettime(CLOCK_MONOTONIC, &start_ts),
			"Client::do_request: clock_gettime");
	}

	bool should_measure = in_count > cfg.pre_samples
		and in_count <= cfg.pre_samples + cfg.samples;
	gen->do_request(should_measure);
}

/**
 * Set timer to fire in usec time *since the last timer firing*
 * (as recorded by last_time). This means if you call timer_arm()
 * with a sequence of usecs based on some probability distribution,
 * the timer will trigger will according to that distribution,
 * and not the distribution + processing time.
 */
void Client::timer_arm(timespec deadline)
{
	itimerspec itval;
	itval.it_interval.tv_sec = 0;
	itval.it_interval.tv_nsec = 0;
	itval.it_value = deadline;
	SystemCall(
		timerfd_settime(timerfd, 0, &itval, NULL),
		"Client::timer_arm: timerfd_settime()");
}

void Client::timer_handler(void)
{
	timespec now_time, result, sleep_time;

	SystemCall(
		clock_gettime(CLOCK_MONOTONIC, &now_time),
		"Client::timer_handler: clock_gettime()");

	timespec_subtract(&now_time, &start_time, &result);

	while(timespec_subtract(&deadlines[in_count], &result, &sleep_time)) {
		do_request();
		in_count++;
		if (in_count > cfg.total_samples) {
			return;
		}
	}

	timer_arm(sleep_time);
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
		timespec now, delta;
		uint64_t delta_us;

		measure_count++;

		SystemCall(
			clock_gettime(CLOCK_MONOTONIC, &now),
			"Client::record_sample: clock_gettime()");

		if (timespec_subtract(&now, &start_ts, &delta)) {
			throw runtime_error("experiment finished before it started");
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

/**
 * Print header for results.
 *
 * NB: Keep this up to date with 'Client::print_summary()'.
 */
void Client::print_header(void)
{
	// TODO: also support detailed sample information
	if (cfg.machine_readable) {
		cout << "label\tservice_us\tarrival_us\tstep_count\trequests_per_sec"
      "\tideal_requests_per_sec\tservice_min\tservice_mean\tservice_stddev"
      "\tservice_99th\tservice_99.9th\tservice_max\twait_min\twait_mean"
      "\twait_stddev\twait_99th\twait_99.9th\twait_max" << endl;
	} else {
		cout << "#reqs/s\t\t(ideal)\t\tmin\tavg\t\tstd\t\t99th\t99.9th"
      "\tmax\tmin\tavg\t\tstd\t\t99th\t99.9th\tmax" << endl;
	}

}

/**
 * Print summary of current results.
 *
 * NB: Keep this up to date with 'Client::print_header()'.
 */
void Client::print_summary(void)
{
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
