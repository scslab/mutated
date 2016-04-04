#ifndef MUTATED_CLIENT2_HH
#define MUTATED_CLIENT2_HH

#include <chrono>
#include <memory>
#include <vector>

#include "accum.hh"
#include "generator.hh"
#include "opts.hh"
#include "socket.hh"

/**
 * Mutated load generator.
 */
class Client
{
  public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;
    using duration = std::chrono::microseconds;

  private:
    Config cfg;

    std::random_device rd;
    std::mt19937 randgen;
    std::uniform_int_distribution<int> conn_dist;
    std::unique_ptr<generator> gen;
    generator::request_cb gen_cb;

    unsigned int epollfd;
    unsigned int timerfd;

    accum service_samples;
    accum wait_samples;
    double throughput;

    uint64_t in_count, out_count, measure_count;
    uint64_t pre_samples, post_samples, measure_samples, total_samples;

    time_point exp_start_time;
    time_point measure_start_time;
    std::vector<duration> deadlines;

    std::vector<Sock *> conns;

    void setup_connections(void);
    Sock *get_connection(void);
    void send_request(void);
    void timer_arm(duration deadline);
    void timer_handler(void);
    void setup_deadlines(void);
    void setup_experiment(void);
    void print_header(void);
    void print_summary(void);

  public:
    /* Create a new client. */
    Client(int argc, char *argv[]);

    /* Destructor. */
    ~Client(void);

    /* No copy or move. */
    Client(const Client &) = delete;
    Client(Client &&) = delete;
    Client &operator=(const Client &) = delete;
    Client &operator=(Client &&) = delete;

    /**
     * epoll_watch - registers a file descriptor for epoll events.
     * @fd: the file descriptor
     * @data: a cookie for the event
     * @event: the event mask
     */
    void epoll_watch(int fd, void *data, uint32_t events);

    /* Run the load generator. */
    void run(void);

    /* Record a latency sample. */
    void record_sample(uint64_t service_us, uint64_t wait_us,
                       bool should_measure);
};

#endif /* MUTATED_CLIENT2_HH */
