#ifndef MUTATED_GENERATOR_HH
#define MUTATED_GENERATOR_HH

#include <chrono>
#include <cstdint>
#include <functional>
#include <random>

#include "opts.hh"
#include "socket_buf.hh"

/**
 * Abstract class defining the interface all load generators must support.
 *
 * We use reference counting to manage a generators life-time. This is
 * 'required' as due to the different orderings in which epoll could fire and a
 * socket be ready for writing, there is at least two different locations at
 * which we may need to call a generators destructor. We also need to be
 * careful to not call the deconstructor in any of the callbacks, as then the
 * 'this' pointer will be corrupted and the caller of the callback will
 * segfault.
 */
class generator
{
  public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;
    using duration = std::chrono::microseconds;
    using request_cb =
      std::function<void(generator *, uint64_t, uint64_t, bool)>;

  protected:
    int ref_cnt_;
    Sock sock_;

    /* Generate requests - internal. */
    virtual void _send_request(bool measure, request_cb cb) = 0;

  public:
    generator(void) noexcept : ref_cnt_{1}, sock_{} {}
    virtual ~generator(void) noexcept {}

    /* No copy or move */
    generator(const generator &) = delete;
    generator(generator &&) = delete;
    generator &operator=(const generator &) = delete;
    generator &operator=(generator &&) = delete;

    /* Take a new reference */
    void get(void) noexcept { ref_cnt_++; }

    /* Release a reference - will deallocate if reference count hits zero */
    void put(void)
    {
        if (--ref_cnt_ == 0) {
            delete this;
        } else if (ref_cnt_ < 0) {
            throw std::runtime_error("generator::put refcnt < 0");
        }
    }

    /* Generate requests */
    void send_request(bool measure, request_cb cb)
    {
        get();
        _send_request(measure, cb);
        put();
    }

    /* Access underlying file descriptor */
    int fd(void) const noexcept { return sock_.fd(); }

    /* Open a new remote connection */
    void connect(const char *addr, unsigned short port)
    {
        sock_.connect(addr, port);
    }

    /* Handle epoll events against this socket */
    void run_io(uint32_t events)
    {
        get();
        sock_.run_io(events);
        put();
    }
};

#endif /* MUTATED_GENERATOR_HH */
