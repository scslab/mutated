#ifndef MUTATED_CLIENT_HH
#define MUTATED_CLIENT_HH

#include <chrono>
#include <memory>
#include <vector>

#include "generator.hh"
#include "opts.hh"
#include "results.hh"

/**
 * Mutated load generator.
 */
class Client
{
  public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;
    using duration = std::chrono::nanoseconds;

  private:
    Config cfg;

    std::random_device rd;
    std::mt19937 randgen;
    std::uniform_int_distribution<int> conn_dist;
    generator::request_cb gen_cb;

    unsigned int epollfd;
    unsigned int timerfd;

    Results results;

    uint64_t sent_count, rcvd_count, measure_count;
    uint64_t pre_samples, post_samples, measure_samples, total_samples;

    time_point exp_start_time;
    std::vector<duration> deadlines;
    duration missed_threshold;
    uint64_t missed_send_window;

    std::vector<generator *> conns;

    generator *new_connection(void);
    void setup_connections(void);
    generator *get_connection(void);
    void send_request(void);
    void epoll_watch(int fd, void *data, uint32_t events);
    void timer_arm(duration deadline);
    void timer_handler(void);
    void setup_deadlines(void);
    void setup_experiment(void);
    void print_summary(void);

  public:
    Client(Config c);
    ~Client(void) noexcept;

    /* No copy or move. */
    Client(const Client &) = delete;
    Client(Client &&) = delete;
    Client &operator=(const Client &) = delete;
    Client &operator=(Client &&) = delete;

    /* Run the load generator. */
    void run(void);

    /* Record a latency sample. */
    void record_sample(generator *, uint64_t queue_us, uint64_t service_us,
                       uint64_t wait_us, uint64_t bytes, bool should_measure);
};

#endif /* MUTATED_CLIENT_HH */
