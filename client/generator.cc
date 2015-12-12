/*
 * generator_reqperflow - creates a new TCP connection for each request
 */
#include <stdexcept>

#include <errno.h>
#include <sys/epoll.h>

#include "generator.hh"
#include "protocol.hh"
#include "socket.hh"
#include "time.hh"
#include "util.hh"

/**
 * Tracks an outstanding request to the service we are generating load against.
 */
class request {
public:
	using request_cb = generator::request_cb;

	bool       should_measure;
	request_cb cb;
	timespec   start_ts;
	uint64_t   service_us;
	req_pkt    req;
	resp_pkt   resp;

	request(bool m, request_cb c, uint64_t service)
		: should_measure{m}, cb{c}, start_ts{}, service_us{service}, req{}
		, resp{}
	{
		SystemCall(
			clock_gettime(CLOCK_MONOTONIC, &start_ts),
			"generator::send_request: clock_gettime()");

		req.nr = 1;
		req.delays[0] = service_us;
	}
};

static void __read_completion_handler(Sock *s, void *data, int status);

/* Constructor */
generator::generator(double service_us, std::mt19937 & rand)
	: rand_{rand}
	, service_dist_{1.0 / service_us}
{
}

/* Return a service time to use for the next request */
uint64_t generator::gen_service_time(void)
{
	return ceil(service_dist_(rand_));
}

void generator::send_request(Sock * sock, bool should_measure, request_cb cb)
{
	// create our request
	request * req = new request(should_measure, cb, gen_service_time());
	req->req.tag = (uint64_t) req;

	// add request to write queue
	vio ent;
	ent.buf = (char *) &req->req;
	ent.len = sizeof(req_pkt);
	ent.complete = nullptr;
	ent.cb_data = nullptr;
	sock->write(ent);

	// add response to read queue
	ent.buf = (char *) &req->resp;
	ent.len = sizeof(resp_pkt);
	ent.cb_data = ent.buf;
	ent.complete = &__read_completion_handler;
	sock->read(ent);
}

static void
__read_completion_handler(Sock *s, void *data, int status)
{
	resp_pkt *resp = (resp_pkt *) data;
	request *req = (request *) resp->tag;
	timespec now, delta;
	uint64_t service_us, wait_us;

	if (status == 0) {
		SystemCall(
			clock_gettime(CLOCK_MONOTONIC, &now),
			"__read_completion_handler: clock_gettime()");

		if (timespec_subtract(&now, &req->start_ts, &delta)) {
			throw std::runtime_error("__read_completion_handler: sample arrived before it was sent");
		}

		service_us = timespec_to_us(&delta);
		// measurement noise can push wait_us into negative values sometimes
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
