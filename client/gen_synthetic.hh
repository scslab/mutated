#ifndef MUTATED_GEN_SYNTHETIC_HH
#define MUTATED_GEN_SYNTHETIC_HH

#include <cstdint>
#include <random>

#include "buffer.hh"
#include "generator.hh"
#include "opts.hh"
#include "socket_buf.hh"

/**
 * Tracks an outstanding synthetic request.
 */
struct synreq {
    using request_cb = generator::request_cb;
    using time_point = generator::time_point;

    bool measure;
    request_cb cb;
    time_point start_ts;
    uint64_t service_us;

    synreq(void) noexcept : synreq(false, nullptr, 0) {}

    synreq(bool m, request_cb c, uint64_t service) noexcept
      : measure{m},
        cb{c},
        start_ts{generator::clock::now()},
        service_us{service}
    {
    }
};

/**
 * Generator supporting our own mutated synthetic protocol.
 */
class synthetic : public generator
{
  public:
    static constexpr std::size_t MAX_OUTSTANDING_REQS = 1024;
    using req_buffer = buffer<synreq, MAX_OUTSTANDING_REQS>;

  private:
    const Config &cfg_;
    std::mt19937 &rand_;
    std::exponential_distribution<double> service_dist_exp;
    std::lognormal_distribution<double> service_dist_lognorm;
    ioop::ioop_cb cb_;
    req_buffer requests;

    uint64_t gen_service_time(void);
    void recv_response(Sock *sock, void *data, char *seg1, size_t n,
                       char *seg2, size_t m, int status);

  protected:
    void _send_request(bool measure, request_cb cb) override;

  public:
    synthetic(const Config &cfg, std::mt19937 &rand) noexcept;
    ~synthetic(void) noexcept {}

    /* No copy or move */
    synthetic(const synthetic &) = delete;
    synthetic(synthetic &&) = delete;
    synthetic &operator=(const synthetic &) = delete;
    synthetic &operator=(synthetic &&) = delete;
};

#endif /* MUTATED_GEN_SYNTHETIC_HH */
