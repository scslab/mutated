#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>

#include <errno.h>

#include "gen_synthetic.hh"
#include "protocol.hh"
#include "socket_buf.hh"
#include "util.hh"

using namespace std;
using namespace std::placeholders;

/**
 * Constructor.
 */
synthetic::synthetic(const Config &cfg, mt19937 &rand) noexcept
  : cfg_(cfg),
    rand_{rand},
    service_dist_exp{1.0 / cfg.service_us},
    service_dist_lognorm{log(cfg.service_us) - 2.0, 2.0},
    cb_{bind(&synthetic::recv_response, this, _1, _2, _3, _4, _5, _6, _7)},
    requests{}
{
}

/**
 * Return a service time to use for the next synreq.
 */
uint64_t synthetic::gen_service_time(void)
{
    if (cfg_.service_dist == cfg_.FIXED) {
        return ceil(cfg_.service_us);
    } else if (cfg_.service_dist == cfg_.EXPONENTIAL) {
        return ceil(service_dist_exp(rand_));
    } else {
        return ceil(service_dist_lognorm(rand_));
    }
}

/**
 * Generate and send a new request.
 */
void synthetic::_send_request(bool measure, request_cb cb)
{
    // create our synreq
    synreq &req = requests.queue_emplace(measure, cb, gen_service_time());
    size_t n = sizeof(req_pkt), n1 = n;
    auto wptrs = sock_.write_prepare(n1);
    if (n1 == n) {
        // contiguous
        req_pkt *pkt = (req_pkt *)wptrs.first;
        pkt->tag = (uint64_t)&req;
        pkt->nr = 1;
        pkt->delays[0] = req.service_us;
    } else {
        // fragmented
        req_pkt pkt;
        pkt.tag = (uint64_t)&req;
        pkt.nr = 1;
        pkt.delays[0] = req.service_us;
        memcpy(wptrs.first, &pkt, n1);
        memcpy(wptrs.second, &pkt + n1, n - n1);
    }
    sock_.write_commit(n);

    // add response to read queue
    ioop io(sizeof(resp_pkt), cb_, 0, nullptr, &req);
    sock_.read(io);
}

/**
 * Handle parsing a response from a previous request.
 */
size_t synthetic::recv_response(Sock *s, void *data, char *seg1, size_t n,
                                char *seg2, size_t m, int status)
{
    UNUSED(seg1);
    UNUSED(seg2);

    if (&sock_ != s) { // ensure right callback
        throw runtime_error("synth::recv_response: wrong socket in callback");
    }

    if (status != 0) { // just drop on error
        requests.drop(1);
        return 0;
    } else if (n + m != sizeof(resp_pkt)) { // ensure valid packet
        throw runtime_error("synth::recv_response: unexpected packet size");
    }

    // parse packet
    const synreq &req = requests.dequeue_one();
    if (data != &req) {
        throw runtime_error(
          "synth::recv_response: wrong response-request packet match");
    }
    auto now = generator::clock::now();
    auto delta = now - req.start_ts;
    if (delta <= generator::duration(0)) {
        throw runtime_error(
          "synth::recv_response: sample arrived before it was sent");
    }

    // measurement noise can push wait_us into negative values sometimes
    uint64_t wait_us;
    uint64_t service_us =
      chrono::duration_cast<generator::duration>(delta).count();
    if (service_us > req.service_us) {
        wait_us = service_us - req.service_us;
    } else {
        wait_us = 0;
    }
    req.cb(this, service_us, wait_us, req.measure);

    // no body, only a header
    return 0;
}
