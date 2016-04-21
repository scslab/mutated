#ifndef MUTATED_RESULTS_HH
#define MUTATED_RESULTS_HH

#include <chrono>
#include <vector>
#include <stdexcept>

#include "accum.hh"
#include "util.hh"

/**
 * Results from a sampling run.
 */
class Results
{
  public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;
    using duration = std::chrono::nanoseconds;

  private:
    time_point measure_start_;
    time_point measure_end_;
    accum service_;
    accum wait_;
    double throughput_;

  public:
    Results(std::size_t reserve) noexcept : measure_start_{}, measure_end_{}, service_{reserve}, wait_{reserve}, throughput_{0} {}

    void start_measurements(void) noexcept
    {
        measure_start_ = clock::now();
    }

    void end_measurements(void) noexcept
    {
        measure_end_ = clock::now();
        auto length = measure_end_ - measure_start_;
        if (length <= clock::duration(0)) {
            throw std::runtime_error("experiment finished before it started");
        }
        double delta_ns = std::chrono::duration_cast<duration>(length).count();
        throughput_ = (double) service_.size() / (delta_ns / NSEC);
    }

    uint64_t running_time(void) noexcept
    {
        auto length = measure_end_ - measure_start_;
        if (length <= clock::duration(0)) {
            throw std::runtime_error("experiment finished before it started");
        }
        return std::chrono::duration_cast<duration>(length).count();
    }

    accum & service(void) noexcept { return service_; }
    accum & wait(void) noexcept { return wait_; }
    double throughput(void) const noexcept { return throughput_; }
};

#endif /* MUTATED_RESULTS_HH */
