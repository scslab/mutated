#ifndef MUTATED_GEN_MEMCACHE_HH
#define MUTATED_GEN_MEMCACHE_HH

#include <cstdint>
#include <random>

#include "generator.hh"
#include "memcache.hh"
#include "opts.hh"
#include "socket_buf.hh"

/**
 * Tracks an outstanding memcache request.
 */
struct memreq {
    using request_cb = generator::request_cb;
    using time_point = generator::time_point;

    bool measure;
    request_cb cb;
    time_point start_ts;

    memreq(void) noexcept : memreq(false, nullptr) {}

    memreq(bool m, request_cb c) noexcept : measure{m},
                                            cb{c},
                                            start_ts{generator::clock::now()}
    {
    }
};

/**
 * Generator supporting the memcache binary protocol.
 */
class memcache : public generator
{
  public:
    static constexpr std::size_t MAX_OUTSTANDING_REQS = 4096;
    using req_buffer = buffer<memreq, MAX_OUTSTANDING_REQS>;

  private:
    const Config &cfg_;
    std::mt19937 &rand_;
    ioop::ioop_cb cb_;
    req_buffer requests_;
    uint64_t seqid_;

    void recv_response(Sock *sock, void *data, char *seg1, size_t n,
                       char *seg2, size_t m, int status);

  protected:
    void _send_request(bool measure, request_cb cb) override;

  public:
    memcache(const Config &cfg, std::mt19937 &rand) noexcept;
    ~memcache(void) noexcept {}

    /* No copy or move */
    memcache(const memcache &) = delete;
    memcache(memcache &&) = delete;
    memcache &operator=(const memcache &) = delete;
    memcache &operator=(memcache &&) = delete;
};

#endif /* MUTATED_GEN_MEMCACHE_HH */
