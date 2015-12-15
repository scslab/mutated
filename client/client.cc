#include <functional>
#include <iostream>

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include "client.hh"
#include "socket.hh"
#include "util.hh"

using namespace std;
using namespace std::placeholders;

/* Microseconds in a second. */
// TODO: Duplicated in opts.cc
static constexpr double USEC = 1000000;

/**
 * Create a new client.
 */
Client::Client(int argc, char *argv[])
	: cfg{argc, argv}
	, rd{}, randgen{rd()}, gen{new generator(cfg.service_us, randgen)}
	, gen_cb{bind(&Client::record_sample, this, _1, _2, _3)}
	, epollfd{SystemCall(epoll_create1(0),
											 "Client::Client: epoll_create1()")}
	, timerfd{SystemCall(timerfd_create(CLOCK_MONOTONIC, O_NONBLOCK),
											 "Client::Client: timefd_create()")}
	, service_samples{}, wait_samples{} , throughput{0}
	, in_count{0}, out_count{0}, measure_count{0}
	, exp_start_time{}, measure_start_time{}
	, deadlines{cfg.total_samples}
{
	epoll_watch(timerfd, NULL, EPOLLIN);
	print_header();
}

/**
 * Destructor.
 */
Client::~Client(void)
{
	SystemCall(close(epollfd), "Client::~Client: close");
	SystemCall(close(timerfd), "Client::~Client: close");
}

/**
 * epoll_watch - registers a file descriptor for epoll events.
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

/**
 * Convert a chrono duration value into a timespec value.
 */
static void __duration_to_timerspec(Client::duration d, timespec & t)
{
	t.tv_sec = d.count() / USEC;
	t.tv_nsec = (d.count() - t.tv_sec * USEC) * 100;
}

/**
 * Set timer to fire in usec time *since the last timer firing*
 * (as recorded by last_time). This means if you call timer_arm()
 * with a sequence of usecs based on some probability distribution,
 * the timer will trigger will according to that distribution,
 * and not the distribution + processing time.
 */
void Client::timer_arm(duration deadline)
{
	itimerspec itval;
	itval.it_interval.tv_sec = 0;
	itval.it_interval.tv_nsec = 0;
	__duration_to_timerspec(deadline, itval.it_value);

	SystemCall(
		timerfd_settime(timerfd, 0, &itval, NULL),
		"Client::timer_arm: timerfd_settime()");
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
				s->run_io(ev.events);
			}
		}
	}
}

void Client::setup_experiment(void)
{
	in_count = out_count = measure_count = 0;
	setup_deadlines();
	exp_start_time = clock::now();
	timer_handler();
}

void Client::setup_deadlines(void)
{
	// Exponential distribution suggested by experimental evidence,
	// c.f. Figure 11 in "Power Management of Online Data-Intensive Services"
	std::exponential_distribution<double> d(1.0 / (USEC / cfg.req_s));

	double accum = 0;
	for (auto & dl : deadlines) {
		accum += d(randgen);
		dl = duration(uint64_t(ceil(accum)));
	}
}

void Client::timer_handler(void)
{
	time_point now_time = clock::now();
	duration now_relative = chrono::duration_cast<duration>(now_time - exp_start_time);

	duration sleep_duration;
	while(true) {
		sleep_duration = deadlines[in_count] - now_relative;
		if (sleep_duration > duration(0)) {
			timer_arm(sleep_duration);
			return;
		}
		send_request();
		in_count++;
		if (in_count >= cfg.total_samples) {
			return;
		}
	}
}

void Client::send_request(void)
{
	if (in_count == cfg.pre_samples) {
		measure_start_time = clock::now();
	}

	// in measure phase? (not warm up or down)
	bool should_measure = in_count >= cfg.pre_samples
		and in_count < cfg.pre_samples + cfg.samples;

	// create a new connection
	Sock * sock = new Sock();
	sock->connect(cfg.addr, cfg.port);
	epoll_watch(sock->fd(), sock, EPOLLIN | EPOLLOUT);

	// sock is reference counted (get/put) and we'll deallocate it in the read
	// callback established by generator.
	gen->send_request(sock, should_measure, gen_cb );
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
		time_point delta;

		measure_count++;

		time_point now = clock::now();
		auto exp_length = now - measure_start_time;
		if (exp_length < clock::duration(0)) {
			throw runtime_error("experiment finished before it started");
		}

		double delta_us = chrono::duration_cast<duration>(exp_length).count();
		throughput = (double) cfg.samples / (delta_us / USEC);
	}

	out_count++;
	if (out_count >= cfg.total_samples) {
		print_summary();
		exit(EXIT_SUCCESS);
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
		cout << "label\tservice_us\tarrival_us\trequests_per_sec"
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
		printf("%s\t%f\t%f\t", // no newline
					 cfg.label,
					 cfg.service_us,
					 USEC / cfg.req_s);
	}

	printf("%f\t%f\t%ld\t%f\t%f\t%ld\t%ld\t%ld\t%ld\t%f\t%f\t%ld\t%ld\t%ld\n",
				 throughput,
				 cfg.req_s,
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
