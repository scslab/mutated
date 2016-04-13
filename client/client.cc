#include <functional>
#include <iostream>

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "client.hh"
#include "generator.hh"
#include "gen_memcache.hh"
#include "gen_synthetic.hh"
#include "socket_buf.hh"
#include "util.hh"

using namespace std;
using namespace std::placeholders;

static int
epoll_spin(int epfd, struct epoll_event *events, int maxevents, int timeout)
{
	return (int) syscall(321, epfd, events, maxevents, timeout);
}

/* Microseconds in a second. */
static constexpr double USEC = 1000000;
static constexpr double NSEC = 1000000000;

/* Return the right protocol generator. */
static generator *choose_generator(const Config &cfg, mt19937 &rand)
{
    switch (cfg.protocol) {
    case Config::SYNTHETIC:
        return new synthetic(cfg, rand);
        break;
    case Config::MEMCACHE:
        return new memcache(cfg);
        break;
    default:
        throw runtime_error("Unknown protocol");
        break;
    }
}

/**
 * Create a new client.
 */
Client::Client(int argc, char *argv[])
  : cfg{argc, argv}
  , rd{}
  , randgen{rd()}
  , conn_dist{0, (int)cfg.conn_cnt - 1}
  , gen{choose_generator(cfg, randgen)}
  , gen_cb{bind(&Client::record_sample, this, _1, _2, _3)}
  , epollfd{SystemCall(epoll_create1(0), "Client::Client: epoll_create1()")}
  , timerfd{SystemCall(timerfd_create(CLOCK_MONOTONIC, O_NONBLOCK),
                       "Client::Client: timefd_create()")}
  , service_samples{}
  , wait_samples{}
  , throughput{0}
  , in_count{0}
  , out_count{0}
  , measure_count{0}
  , pre_samples{0}
  , post_samples{0}
  , measure_samples{0}
  , total_samples{0}
  , exp_start_time{}
  , measure_start_time{}
  , deadlines{}
  , conns{cfg.conn_cnt}
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
    SystemCall(epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev),
               "Client::epoll_watch: epoll_ctl()");
}

/**
 * Convert a chrono duration value into a timespec value.
 */
static void __duration_to_timerspec(Client::duration d, timespec &t)
{
    t.tv_sec = d.count() / NSEC;
    t.tv_nsec = (d.count() - t.tv_sec * NSEC);
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

    SystemCall(timerfd_settime(timerfd, 0, &itval, NULL),
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
        int nfds;

        if (cfg.use_epoll_spin) {
            nfds = SystemCall(epoll_spin(epollfd, events, MAX_EVENTS, -1),
                              "Client::run: epoll_spin()");
        } else {
            nfds = SystemCall(epoll_wait(epollfd, events, MAX_EVENTS, -1),
                              "Client::run: epoll_wait()");
        }

        for (int i = 0; i < nfds; i++) {
            epoll_event &ev = events[i];

            /* is it a timer? */
            if (ev.data.ptr == nullptr) {
                timer_handler();
            } else {
                Sock *s = (Sock *)ev.data.ptr;
                s->run_io(ev.events);
            }
        }
    }
}

void Client::setup_experiment(void)
{
    in_count = out_count = measure_count = 0;
    setup_connections();
    setup_deadlines();
    exp_start_time = clock::now();
    timer_handler();
}

void Client::setup_deadlines(void)
{
    // Exponential distribution suggested by experimental evidence,
    // c.f. Figure 11 in "Power Management of Online Data-Intensive Services"
    std::exponential_distribution<double> d(1.0 / (NSEC / cfg.req_s));
    double accum = 0;
    uint64_t pos = 0;
    duration coolstart;

    // generate warm-up samples
    while (duration(uint64_t(ceil(accum))) <
           std::chrono::seconds(cfg.warmup_seconds)) {
        accum += d(randgen);
        deadlines.push_back(duration(uint64_t(ceil(accum))));
        pos++;
    }
    pre_samples = pos;

    // generate measurement samples
    while (pos - pre_samples < cfg.samples) {
        accum += d(randgen);
        deadlines.push_back(duration(uint64_t(ceil(accum))));
        pos++;
    }
    measure_samples = cfg.samples;

    // generate cool-down samples
    coolstart = duration(uint64_t(ceil(accum)));
    while (duration(uint64_t(ceil(accum))) - coolstart <
           std::chrono::seconds(cfg.cooldown_seconds)) {
        accum += d(randgen);
        deadlines.push_back(duration(uint64_t(ceil(accum))));
        pos++;
    }
    post_samples = pos - pre_samples - measure_samples;
    total_samples = pos;
}

void Client::setup_connections(void)
{
    if (cfg.conn_mode == cfg.PER_REQUEST) {
        return;
    }

    for (auto &sock : conns) {
        sock = new Sock();
        sock->connect(cfg.addr, cfg.port);
        epoll_watch(sock->fd(), sock, EPOLLIN | EPOLLOUT);
    }
}

void Client::teardown_connections(void)
{
    if (cfg.conn_mode == cfg.PER_REQUEST)
        return;

    for (auto &sock : conns) {
        sock->put();
    }
}

Sock *Client::get_connection(void)
{
    Sock *sock;

    if (cfg.conn_mode == cfg.PER_REQUEST) {
        // create a new connection per request
        sock = new Sock();
        sock->connect(cfg.addr, cfg.port);
        epoll_watch(sock->fd(), sock, EPOLLIN | EPOLLOUT);
        return sock;
    } else if (cfg.conn_mode == cfg.ROUND_ROBIN) {
        // round-robin through a pool of established connections
        static int idx = 0;
        sock = conns[idx++ % conns.size()];
        sock->get(); // indicate start of request
        return sock;
    } else {
        // randomly choose a connection from the pool
        sock = conns[conn_dist(randgen)];
        sock->get(); // indicate start of request
        return sock;
    }
}

void Client::timer_handler(void)
{
    time_point now_time = clock::now();
    duration now_relative =
      chrono::duration_cast<duration>(now_time - exp_start_time);

    duration sleep_duration;
    while (true) {
        sleep_duration = deadlines[in_count] - now_relative;
        if (sleep_duration > duration(0)) {
            timer_arm(sleep_duration);
            return;
        }
        send_request();
        in_count++;
        if (in_count >= total_samples) {
            return;
        }
    }
}

void Client::send_request(void)
{
    if (in_count == pre_samples) {
        measure_start_time = clock::now();
    }

    // in measure phase? (not warm up or down)
    bool should_measure =
      in_count >= pre_samples and in_count < pre_samples + measure_samples;

    // get an available connection
    Sock *sock = get_connection();

    // sock is reference counted (get/put) and we'll deallocate it in the read
    // callback established by generator.
    gen->send_request(sock, should_measure, gen_cb);
}

/* Record a latency sample */
void Client::record_sample(uint64_t service_us, uint64_t wait_us,
                           bool should_measure)
{
    if (should_measure) {
        measure_count++;
        service_samples.add_sample(service_us);
        wait_samples.add_sample(wait_us);
    }

    if (measure_count == measure_samples) {
        time_point delta;

        measure_count++;

        time_point now = clock::now();
        auto exp_length = now - measure_start_time;
        if (exp_length < clock::duration(0)) {
            throw runtime_error("experiment finished before it started");
        }

        double delta_ns = chrono::duration_cast<duration>(exp_length).count();
        throughput = (double)cfg.samples / (delta_ns / NSEC);
    }

    out_count++;
    if (out_count >= total_samples) {
	teardown_connections();
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
    if (!cfg.machine_readable) {
        cout << "#reqs/s\t\t(ideal)\t\tmin\tavg\t\tstd\t\t99th\t99.9th"
                "\tmax\tmin\tavg\t\tstd\t\t99th\t99.9th\tmax"
             << endl;
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
        service_samples.print_samples();
        return;
    }

    printf("%f\t%f\t%ld\t%f\t%f\t%ld\t%ld\t%ld\t%ld\t%f\t%f\t%ld\t%ld\t%ld\n",
           throughput, cfg.req_s, service_samples.min(),
           service_samples.mean(), service_samples.stddev(),
           service_samples.percentile(0.99), service_samples.percentile(0.999),
           service_samples.max(), wait_samples.min(), wait_samples.mean(),
           wait_samples.stddev(), wait_samples.percentile(0.99),
           wait_samples.percentile(0.999), wait_samples.max());
}
