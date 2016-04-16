#include <chrono>
#include <functional>
#include <iostream>
#include <stdexcept>

#include <errno.h>

#include "gen_memcache.hh"
#include "socket_buf.hh"
#include "util.hh"

using namespace std;
using namespace std::placeholders;

/* GET request. */
static const char *getreq = "get a\r\n";

/**
 * Construct.
 */
memcache::memcache(const Config &cfg) noexcept
  : cfg_{cfg},
    cb_{bind(&memcache::recv_response, this, _1, _2, _3, _4, _5, _6, _7)},
    requests_{}
{
    UNUSED(cfg_);
}

/**
 * Generate and send a new request.
 */
void memcache::_send_request(bool measure, request_cb cb)
{
    // create our request
    memreq *req = new memreq(measure, cb);

    // add req to write queue
    size_t n = strlen(getreq), n1 = n;
    auto wptrs = sock_.write_prepare(n1);
    memcpy(wptrs.first, getreq, n1);
    if (n != n1) {
        memcpy(wptrs.second, getreq + n1, n - n1);
    }
    sock_.write_commit(n);

    // add response to read queue
    ioop io(5, req, cb_);
    sock_.read(io);
}

/**
 * Handle parsing a response from a previous request.
 */
void memcache::recv_response(Sock *s, void *data, char *seg1, size_t n,
                             char *seg2, size_t m, int status)
{
    UNUSED(seg1);
    UNUSED(seg2);

    if (&sock_ != s) { // ensure right callback
        throw runtime_error("synth::recv_response: wrong socket in callback");
    }

    if (status != 0) { // just delete on error
        delete reinterpret_cast<memreq *>(data);
        return;
    } else if (n + m != 5) { // ensure valid packet
        throw runtime_error("memcache::recv_response: unexpected packet size");
    }

    // parse packet
    memreq *req = reinterpret_cast<memreq *>(data);
    auto now = generator::clock::now();
    auto delta = now - req->start_ts;
    if (delta <= generator::duration(0)) {
        throw std::runtime_error(
          "__read_completion_handler: sample arrived before it was sent");
    }
    uint64_t service_us =
      chrono::duration_cast<generator::duration>(delta).count();

    // record measurement
    req->cb(this, service_us, 0, req->measure);
    delete req;
}
