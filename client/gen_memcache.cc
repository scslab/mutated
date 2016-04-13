#include <chrono>
#include <iostream>
#include <stdexcept>

#include <errno.h>
#include <sys/epoll.h>

#include "gen_memcache.hh"
#include "socket_buf.hh"
#include "util.hh"

using namespace std;

static constexpr size_t MAX_MEM_RESP = 100;

/**
 * Tracks an outstanding memcache request.
 */
struct memreq {
    using request_cb = generator::request_cb;
    using time_point = generator::time_point;

    bool measure;
    request_cb cb;
    time_point start_ts;
    char resp[MAX_MEM_RESP];

    memreq(bool m, request_cb c)
      : measure{m}
      , cb{c}
      , start_ts{generator::clock::now()}
    {
    }
};

static void __read_completion_handler(Sock *, char *, size_t, char *, size_t,
                                      void *, int);

static const char *getreq = "get a\r\n";

/**
 * Constructor.
 */
memcache::memcache(const Config &cfg) noexcept : cfg_(cfg) {}

/**
 * Generate and send a new request.
 */
void memcache::send_request(bool measure, request_cb cb)
{
    // create our request
    memreq *req = new memreq(measure, cb);

    // add req to write queue
    size_t n = strlen(getreq), n1 = n;
    auto wptrs = soc.> write_prepare(n1);
    memcpy(wptrs.first, getreq, n1);
    if (n != n1) {
        memcpy(wptrs.second, getreq + n1, n - n1);
    }
    sock.write_commit(n);

    // add response to read queue
    ioop io(5, req, &__read_completion_handler);
    sock.read(io);
}

/**
 * Handle parsing a memcache response from a previous request.
 */
static void __read_completion_handler(Sock *sock, char *seg1, size_t n,
                                      char *seg2, size_t m, void *data,
                                      int status)
{
    UNUSED(seg1);
    UNUSED(seg2);

    if (n + m != 5) { // ensure valid packet
        throw runtime_error(
          "__read_completion_handler: unexpected packet size");
    } else if (status != 0) { // just delete on error
        delete (memreq *)data;
        sock->put();
        return;
    }

    // parse packet
    memreq *req = (memreq *)data;
    auto now = generator::clock::now();
    auto delta = now - req->start_ts;
    if (delta <= generator::duration(0)) {
        throw std::runtime_error(
          "__read_completion_handler: sample arrived before it was sent");
    }
    uint64_t service_us =
      chrono::duration_cast<generator::duration>(delta).count();

    // record measurement
    req->cb(service_us, 0, req->measure);

    delete req;
    sock->put(); // indicate end of request
}
