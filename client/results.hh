#ifndef MUTATED_RESULTS_HH
#define MUTATED_RESULTS_HH

#include "accum.hh"

/**
 * Results from a sampling run.
 */
class Results
{
private:
    accum service_;
    accum wait_;
    double throughput_;

public:
    Results(std::size_t reserve) noexcept : service_{reserve}, wait_{reserve}, throughput_{0} {}

    accum & service(void) noexcept { return service_; }
    accum & wait(void) noexcept { return wait_; }
    double & throughput(void) noexcept { return throughput_; }
};

#endif /* MUTATED_RESULTS_HH */
