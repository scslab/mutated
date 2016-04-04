/*
 * generator_reqperflow - creates a new TCP connection for each request
 */
#include <chrono>
#include <stdexcept>

#include <errno.h>
#include <sys/epoll.h>

#include "generator.hh"
#include "protocol.hh"
#include "socket.hh"
#include "util.hh"

using namespace std;

/**
 * Tracks an outstanding request to the service we are generating load against.
 */
class request
{
  public:
    using request_cb = generator::request_cb;
    using time_point = generator::time_point;

    bool should_measure;
    request_cb cb;
    time_point start_ts;
    uint64_t service_us;
    req_pkt req;
    resp_pkt resp;

    request(bool m, request_cb c, uint64_t service)
      : should_measure{m}
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

static void __read_completion_handler(Sock *s, void *data, int status);

/* Constructor */
generator::generator(std::mt19937 &rand, Config &cfg)
  : rand_{rand}
  , service_dist_exp{1.0 / cfg.service_us}
  , service_dist_lognorm{log(cfg.service_us) - 2.0, 2.0}
  , cfg_(cfg)
{
}

/* Return a service time to use for the next request */
uint64_t generator::gen_service_time(void)
{
    if (cfg_.service_dist == cfg_.FIXED)
	return ceil(cfg_.service_us);
    else if (cfg_.service_dist == cfg_.EXPONENTIAL)
        return ceil(service_dist_exp(rand_));
    else
        return ceil(service_dist_lognorm(rand_));
}

void generator::send_request(Sock *sock, bool should_measure, request_cb cb)
{
    // create our request
    request *req = new request(should_measure, cb, gen_service_time());
    req->req.tag = (uint64_t)req;

    // add request to write queue
    vio ent((char *)&req->req, sizeof(req_pkt));
    sock->write(ent);

    // add response to read queue
    ent.buf = (char *)&req->resp;
    ent.len = sizeof(resp_pkt);
    ent.cb_data = ent.buf;
    ent.complete = &__read_completion_handler;
    sock->read(ent);
}

static void __read_completion_handler(Sock *s, void *data, int status)
{
    resp_pkt *resp = (resp_pkt *)data;
    request *req = (request *)resp->tag;
    uint64_t wait_us;

    if (status == 0) {
        auto now = generator::clock::now();
        auto delta = now - req->start_ts;
        if (delta <= generator::duration(0)) {
            throw std::runtime_error(
              "__read_completion_handler: sample arrived before it was sent");
        }

        // measurement noise can push wait_us into negative values sometimes
        uint64_t service_us =
          chrono::duration_cast<generator::duration>(delta).count();
        if (service_us > req->service_us) {
            wait_us = service_us - req->service_us;
        } else {
            wait_us = 0;
        }

        req->cb(service_us, wait_us, req->should_measure);
    }

    delete req;
    s->put();
}
