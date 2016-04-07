#ifndef MUTATED_GEN_SYNTHETIC_HH
#define MUTATED_GEN_SYNTHETIC_HH

#include <cstdint>
#include <random>

#include "generator.hh"
#include "opts.hh"
#include "socket.hh"

/**
 * Generator supporting our own mutated synthetic protocol.
 */
class synthetic : public generator
{
  private:
    const Config &cfg_;
    std::mt19937 &rand_;
    std::exponential_distribution<double> service_dist_exp;
    std::lognormal_distribution<double> service_dist_lognorm;

    uint64_t gen_service_time(void);

  public:
    /* Constructor */
    synthetic(const Config &cfg, std::mt19937 &rand);
    ~synthetic(void) = default;

    /* No copy or move */
    synthetic(const synthetic &) = delete;
    synthetic(synthetic &&) = delete;
    synthetic &operator=(const synthetic &) = delete;
    synthetic &operator=(synthetic &&) = delete;

    /* Generate requests */
    void send_request(Sock *sock, bool should_measure, request_cb cb) override;
};

#endif /* MUTATED_GEN_SYNTHETIC_HH */
