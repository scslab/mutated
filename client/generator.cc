/*
 * generator_reqperflow - creates a new TCP connection for each request
 */
#include <stdexcept>

#include <errno.h>
#include <sys/epoll.h>

#include "client.hh"
#include "generator.hh"
#include "protocol.hh"
#include "socket.hh"
#include "time.hh"
#include "util.hh"

/**
 * Tracks an outstanding request to the service we are generating load against.
 */
struct request {
	bool      should_measure;
	timespec  start_ts;
	uint64_t  service_us;
	req_pkt   req;
	resp_pkt  resp;
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

void generator::do_request(bool should_measure)
{
	sg_ent ent;
	request *req;

	// create a new connection
	Sock *s = new Sock();
	// TODO: Cleaner than global client_
	s->connect(client_->addr(), client_->port());

	// register for epoll events on this socket
	// TODO: Cleaner than global client_
	client_->epoll_watch(s->fd(), s, EPOLLIN | EPOLLOUT);

	// create our request
	req = new request();
	SystemCall(
		clock_gettime(CLOCK_MONOTONIC, &req->start_ts),
		"generator::do_request: clock_gettime()");
	req->should_measure = should_measure;
	req->service_us = gen_service_time();
	req->req.nr = 1;
	req->req.tag = (uint64_t) req;
	req->req.delays[0] = req->service_us;

	// add request to write queue
	ent.buf = (char *) &req->req;
	ent.len = sizeof(req_pkt);
	ent.complete = nullptr;
	ent.cb_data = nullptr;
	s->write(ent);

	// add response to read queue
	ent.buf = (char *) &req->resp;
	ent.len = sizeof(resp_pkt);
	ent.cb_data = ent.buf;
	ent.complete = &__read_completion_handler;
	s->read(ent);

	// XXX: Seems like a memory-leak of 's' here, but we are still using it in
	// the '__read_completion_handler' (and using reference counts to track).
}

static void __read_completion_handler(Sock *s, void *data, int status)
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

		// TODO: Cleaner than global client_
		client_->record_sample(service_us, wait_us, req->should_measure);
	}

	delete req;
	s->put();
}
