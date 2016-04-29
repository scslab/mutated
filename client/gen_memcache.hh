#ifndef MUTATED_GEN_MEMCACHE_HH
#define MUTATED_GEN_MEMCACHE_HH

#include <cstdint>
#include <random>

#include "generator.hh"
#include "limits.hh"
#include "memcache.hh"
#include "opts.hh"
#include "socket_buf.hh"

/**
 * Tracks an outstanding memcache request.
 */
struct memreq {
    using request_cb = generator::request_cb;
    using time_point = generator::time_point;

    MemcCmd op;
    bool measure;
    request_cb cb;
    time_point start_ts;
    time_point sent_ts;

    memreq(void) noexcept : memreq(MemcCmd::Get, false, nullptr) {}

    memreq(MemcCmd o, bool m, request_cb c) noexcept
      : op{o},
        measure{m},
        cb{c},
        start_ts{},
        sent_ts{}
    {
    }
};

/**
 * Generator supporting the memcache binary protocol.
 */
class memcache : public generator
{
  public:
    using req_buffer = buffer<memreq, MAX_OUTSTANDING_REQS>;

  private:
    const Config &cfg_;
    std::mt19937 rand_;
    std::uniform_real_distribution<> setget_;
    ioop_rx::ioop_cb rcb_;
    ioop_tx::ioop_cb tcb_;
    req_buffer requests_;
    uint64_t seqid_;

    MemcCmd choose_cmd(void);
    char *choose_key(uint64_t id, uint16_t &n);
    char *choose_val(uint64_t id, uint32_t &n);

    void sent_request(Sock *s, void *data, int status);
    size_t recv_response(Sock *sock, void *data, char *seg1, size_t n,
                         char *seg2, size_t m, int status);

  protected:
    uint64_t _send_request(bool measure, request_cb cb) override;

  public:
    memcache(const Config &cfg, std::mt19937 &&rand) noexcept;
    ~memcache(void) noexcept {}

    /* No copy or move */
    memcache(const memcache &) = delete;
    memcache(memcache &&) = delete;
    memcache &operator=(const memcache &) = delete;
    memcache &operator=(memcache &&) = delete;
};

#endif /* MUTATED_GEN_MEMCACHE_HH */
