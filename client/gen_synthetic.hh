#ifndef MUTATED_GEN_SYNTHETIC_HH
#define MUTATED_GEN_SYNTHETIC_HH

#include <cstdint>
#include <random>

#include "generator.hh"
#include "opts.hh"
#include "socket_buf.hh"

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
    ioop::ioop_cb cb_;

    uint64_t gen_service_time(void);
    void recv_response(Sock *sock, void *data, char *seg1, size_t n,
                       char *seg2, size_t m, int status);

  protected:
    void _send_request(bool measure, request_cb cb) override;

  public:
    /* Constructor */
    synthetic(const Config &cfg, std::mt19937 &rand) noexcept;
    ~synthetic(void) noexcept = default;

    /* No copy or move */
    synthetic(const synthetic &) = delete;
    synthetic(synthetic &&) = delete;
    synthetic &operator=(const synthetic &) = delete;
    synthetic &operator=(synthetic &&) = delete;
};

#endif /* MUTATED_GEN_SYNTHETIC_HH */
