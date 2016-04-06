#ifndef MUTATED_GENERATOR_HH
#define MUTATED_GENERATOR_HH

/**
 * generator.h - support for load generators
 */

#include <chrono>
#include <cstdint>
#include <functional>
#include <random>

#include "opts.hh"
#include "socket.hh"

/**
 * Abstract class defining the interface all generators must support.
 */
class generator
{
  public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;
    using duration = std::chrono::microseconds;
    using request_cb = std::function<void(uint64_t, uint64_t, bool)>;
    /* void (service_us uint64_t, wait_us uint64_t, measure bool) */

    /* Constructor */
    generator(void) = default;
    virtual ~generator(void) = default;

    /* No copy or move */
    generator(const generator &) = delete;
    generator(generator &&) = delete;
    generator &operator=(const generator &) = delete;
    generator &operator=(generator &&) = delete;

    /* Generate requests */
    virtual void send_request(Sock *sock, bool should_measure, request_cb cb) = 0;
};

#endif /* MUTATED_GENERATOR_HH */
