#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>

#include <errno.h>
#include <sys/epoll.h>

#include "gen_synthetic.hh"
#include "protocol.hh"
#include "socket_buf.hh"
#include "util.hh"

using namespace std;

/**
 * Tracks an outstanding synthetic request.
 */
struct synreq
{
    using request_cb = generator::request_cb;
    using time_point = generator::time_point;

    bool measure;
    request_cb cb;
    time_point start_ts;
    uint64_t service_us;
    req_pkt req;
    resp_pkt resp;

    synreq(bool m, request_cb c, uint64_t service)
      : measure{m}
      , cb{c}
      , start_ts{generator::clock::now()}
      , service_us{service}
      , req{}
      , resp{}
    {
        req.nr = 1;
        req.delays[0] = service_us;
    }
};

static void __read_completion_handler(Sock *, char *, size_t, char *, size_t,
                                      void *, int);

/* Constructor */
synthetic::synthetic(const Config &cfg, mt19937 &rand)
  : cfg_(cfg)
  , rand_{rand}
  , service_dist_exp{1.0 / cfg.service_us}
  , service_dist_lognorm{log(cfg.service_us) - 2.0, 2.0}
{
}

/* Return a service time to use for the next synreq */
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

/* Generate and send a new request */
void synthetic::send_request(Sock *sock, bool measure, request_cb cb)
{
    // create our synreq
    synreq *req = new synreq(measure, cb, gen_service_time());
    req->req.tag = (uint64_t)req; // store pointer to ourselves in tag

    // add synreq to write queue
    size_t n = sizeof(req_pkt), n1 = n;
    auto wptrs = sock->write_prepare(n1);
    memcpy(wptrs.first, &req->req, n1);
    if (n != n1) {
        memcpy(wptrs.second, &req->req + n1, n - n1);
    }
    sock->write_commit(n);

    // add response to read queue
    ioop io(sizeof(resp_pkt), req, &__read_completion_handler);
    sock->read(io);
}

/* Handle parsing a response from a previous request */
static void __read_completion_handler(Sock *sock, char *seg1, size_t n,
                                      char *seg2, size_t m, void *data,
                                      int status)
{
    UNUSED(seg1);
    UNUSED(seg2);

    if (n + m != sizeof(resp_pkt)) { // ensure valid packet
        throw runtime_error(
          "__read_completion_handler: unexpected packet size");
    } else if (status != 0) { // just delete on error
        delete (synreq *)data;
        sock->put();
        return;
    }

    // parse packet
    synreq *req = (synreq *)data;
    auto now = generator::clock::now();
    auto delta = now - req->start_ts;
    if (delta <= generator::duration(0)) {
        throw runtime_error(
          "__read_completion_handler: sample arrived before it was sent");
    }

    // measurement noise can push wait_us into negative values sometimes
    uint64_t wait_us;
    uint64_t service_us =
      chrono::duration_cast<generator::duration>(delta).count();
    if (service_us > req->service_us) {
        wait_us = service_us - req->service_us;
    } else {
        wait_us = 0;
    }
    req->cb(service_us, wait_us, req->measure);

    delete req;
    sock->put(); // indicate end of request
}
