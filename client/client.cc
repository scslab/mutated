#include <functional>
#include <iostream>

#include <inttypes.h>
#include <fcntl.h>

#include "client.hh"
#include "gen_memcache.hh"
#include "gen_synthetic.hh"
#include "generator.hh"
#include "linux_compat.hh"
#include "socket_buf.hh"
#include "util.hh"

using namespace std;
using namespace std::placeholders;

/**
 * Create a new client.
 */
Client::Client(Config c)
  : cfg{c}
  , rd{}
  , randgen{rd()}
  , conn_dist{0, (int)cfg.conn_cnt - 1}
  , gen_cb{bind(&Client::record_sample, this, _1, _2, _3, _4, _5, _6)}
  , epollfd{SystemCall(epoll_create1(0), "Client::Client: epoll_create1()")}
  , timerfd{SystemCall(timerfd_create(CLOCK_MONOTONIC, O_NONBLOCK),
                       "Client::Client: timefd_create()")}
  , results{cfg.samples}
  , sent_count{0}
  , rcvd_count{0}
  , measure_count{0}
  , pre_samples{0}
  , post_samples{0}
  , measure_samples{0}
  , total_samples{0}
  , exp_start_time{}
  , deadlines{}
  , missed_threshold{-cfg.missed_window_us * 1000}
  , missed_send_window{0}
  , conns{}
{
    epoll_watch(timerfd, NULL, EPOLLIN);
}

/**
 * Destructor.
 */
Client::~Client(void) noexcept
{
    close(epollfd);
    close(timerfd);
}

/**
 * epoll_watch - registers a file descriptor for epoll events.
 * @fd: the file descriptor.
 * @data: a cookie for the event.
 * @event: the event mask.
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
    // Maximum outstanding epoll events supported
    constexpr size_t MAX_EVENTS = 4096;
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

            // is it a timer?
            if (ev.data.ptr == nullptr) {
                timer_handler();
            } else {
                generator *g = reinterpret_cast<generator *>(ev.data.ptr);
                g->run_io(ev.events);
            }
        }
    }
}

void Client::setup_experiment(void)
{
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

/**
 * Create a new socket and associated packet generator.
 */
generator *Client::new_connection(void)
{
    generator *gen;
    switch (cfg.protocol) {
    case Config::SYNTHETIC:
        gen = new synthetic(cfg, randgen);
        break;
    case Config::MEMCACHE:
        gen = new memcache(cfg, mt19937(rd()));
        break;
    default:
        throw runtime_error("Unknown protocol");
        break;
    }

    gen->connect(cfg.addr, cfg.port);
    epoll_watch(gen->fd(), gen, EPOLLIN | EPOLLOUT);
    return gen;
}

void Client::setup_connections(void)
{
    if (cfg.conn_mode == cfg.PER_REQUEST) {
        return;
    }

    for (size_t i = 0; i < cfg.conn_cnt; i++) {
        conns.push_back(new_connection());
    }
}

generator *Client::get_connection(void)
{
    if (cfg.conn_mode == cfg.PER_REQUEST) {
        // create a new connection per request
        return new_connection();
    } else if (cfg.conn_mode == cfg.ROUND_ROBIN) {
        // round-robin through a pool of established connections
        static int idx = 0;
        generator *gen = conns[idx++ % conns.size()];
        gen->get();
        return gen;
    } else {
        // randomly choose a connection from the pool
        generator *gen = conns[conn_dist(randgen)];
        gen->get();
        return gen;
    }
}

void Client::timer_handler(void)
{
    time_point now_time = clock::now();
    duration now_relative =
      chrono::duration_cast<duration>(now_time - exp_start_time);

    uint64_t looped = 0;
    duration sleep_duration;
    while (true) {
        sleep_duration = deadlines[sent_count] - now_relative;
        if (sleep_duration > duration(0)) {
            timer_arm(sleep_duration);
            return;
        } else if (looped > 0 and sleep_duration < missed_threshold) {
            // Missed timer! Client appears to be overloaded...
            // TODO: Best way to detect and record?
            missed_send_window++;
        }
        looped++;
        send_request();
        sent_count++;
        if (sent_count >= total_samples) {
            return;
        }
    }
}

void Client::send_request(void)
{
    if (sent_count == pre_samples) {
        results.start_measurements();
    }

    // in measure phase? (not warm up or down)
    bool measure =
      sent_count >= pre_samples and sent_count < pre_samples + measure_samples;

    // gen is reference counted (get/put, starts at 1) and we'll deallocate it
    // in `record_sample`.
    generator *gen = get_connection();
    uint64_t bytes = gen->send_request(measure, gen_cb);
    if (measure) {
        results.sent_bytes(bytes);
    }
}

/**
 * Record a latency sample.
 */
void Client::record_sample(generator *conn, uint64_t queue_us, uint64_t service_us,
                           uint64_t wait_us, uint64_t bytes, bool measure)
{
    if (measure) {
        measure_count++;
        results.add_sample(queue_us, service_us, wait_us, bytes);

        // final measurement app-packet - record experiment time
        if (measure_count == measure_samples) {
            results.end_measurements();
        }
    }

    conn->put(); // request finished

    rcvd_count++;
    if (rcvd_count >= total_samples) {
        print_summary();
        exit(EXIT_SUCCESS);
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
        printf("%f\n", results.reqps());
        results.service().print_samples();
        return;
    }

    cout << "#reqs/s: hit\t\ttarget" << endl;
    printf("         %f\t%f\t\n", results.reqps(), cfg.req_s);
    cout << endl;

    cout << "service: min\tavg\t\tstd\t\t99th\t99.9th\tmax" << endl;
    printf("         %" PRIu64 "\t%f\t%f\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64
           "\n",
           results.service().min(), results.service().mean(),
           results.service().stddev(), results.service().percentile(0.99),
           results.service().percentile(0.999), results.service().max());
    cout << endl;

    cout << " buffer: min\tavg\t\tstd\t\t99th\t99.9th\tmax" << endl;
    printf("         %" PRIu64 "\t%f\t%f\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64
           "\n",
           results.queue().min(), results.queue().mean(),
           results.queue().stddev(), results.queue().percentile(0.99),
           results.queue().percentile(0.999), results.queue().max());

    if (cfg.protocol == Config::SYNTHETIC) {
        cout << endl;
        cout << "   wait: min\tavg\t\tstd\t\t99th\t99.9th\tmax" << endl;
        printf("         %" PRIu64 "\t%f\t%f\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64
               "\n",
               results.wait().min(), results.wait().mean(),
               results.wait().stddev(), results.wait().percentile(0.99),
               results.wait().percentile(0.999), results.wait().max());
    }

    constexpr uint64_t MB = 1024 * 1024;
    double time_s = results.running_time() / NSEC;
    double rx_mbs = double(results.rx_bytes()) / MB;
    double tx_mbs = double(results.tx_bytes()) / MB;

    printf("\n");
    printf("RX: %.2f MB/s (%.2f MB)\n", rx_mbs / time_s, rx_mbs);
    printf("TX: %.2f MB/s (%.2f MB)\n", tx_mbs / time_s, tx_mbs);
    printf("Missed sends: %lu / %lu (%.4f%%)\n", missed_send_window, sent_count, double(missed_send_window) / sent_count * 100);
}
