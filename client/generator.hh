#ifndef MUTATED_GENERATOR_HH
#define MUTATED_GENERATOR_HH

/**
 * generator.h - support for load generators
 */

#include <cstdint>
#include <functional>
#include <random>

#include "socket.hh"

class generator
{
  public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;
    using duration = std::chrono::microseconds;
    using request_cb = std::function<void(uint64_t, uint64_t, bool)>;

  private:
    std::mt19937 &rand_;
    std::exponential_distribution<double> service_dist_;

    uint64_t gen_service_time(void);

  public:
    /* Constructor */
    generator(double service_us, std::mt19937 &rand);

    /* No copy or move */
    generator(const generator &) = delete;
    generator(generator &&) = delete;
    generator &operator=(const generator &) = delete;
    generator &operator=(generator &&) = delete;

    /* Generate requests */
    void send_request(Sock *sock, bool should_measure, request_cb cb);
};

#endif /* MUTATED_GENERATOR_HH */
