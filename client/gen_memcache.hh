#ifndef MUTATED_GEN_MEMCACHE_HH
#define MUTATED_GEN_MEMCACHE_HH

#include <cstdint>

#include "generator.hh"
#include "opts.hh"
#include "socket_buf.hh"

/**
 * Generator supporting the memcache binary protocol.
 */
class memcache : public generator
{
  private:
    const Config &cfg_;

  public:
    explicit memcache(const Config &cfg) noexcept;
    ~memcache(void) noexcept {}

    /* No copy or move */
    memcache(const memcache &) = delete;
    memcache(memcache &&) = delete;
    memcache &operator=(const memcache &) = delete;
    memcache &operator=(memcache &&) = delete;

    /* Generate requests */
    void send_request(bool measure, request_cb cb) override;
};

#endif /* MUTATED_GEN_MEMCACHE_HH */
