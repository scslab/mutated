/*
 * generator_reqperflow - creates a new TCP connection for each request
 */

#include <errno.h>

#include "client.hh"
#include "debug.hh"
#include "generator.hh"
#include "socket.hh"
#include "time.hh"
#include "protocol.hh"

int generator::start(void)
{
	return 0;
}

struct request {
	bool		should_measure;
	struct		timespec start_ts;
	uint64_t	service_us;
	struct req_pkt	req;
	struct resp_pkt resp;
};

static void read_completion_handler(Sock *s, void *data, int status)
{
	struct resp_pkt *resp = (struct resp_pkt *) data;
	struct request *req = (struct request *) resp->tag;
	struct timespec now, delta;
	int ret;
	uint64_t service_us, wait_us;

	if (status) {
		/* FIXME: free memory */
		return;
	}

	ret = clock_gettime(CLOCK_MONOTONIC, &now);
	if (ret == -1) {
		perror("clock_gettime()");
		panic("read_completion_handler: clock_gettime()");
	}

	if (timespec_subtract(&now, &req->start_ts, &delta)) {
		panic("read_completion_handler: sample arrived before it was sent");
	}

	service_us = timespec_to_us(&delta);
	// measurement noise can push wait_us into negative values sometimes
	if (service_us > req->service_us)
		wait_us = service_us - req->service_us;
	else
		wait_us = 0;

	client_->record_sample(service_us, wait_us, req->should_measure);

	delete req;
  s->put();
}

int generator::do_request(bool should_measure)
{
	std::exponential_distribution<> d(1 / (double) client_->service_us());
	struct sg_ent ent;
	struct request *req;
	Sock *s = new Sock();
	if (!s) {
		return -ENOMEM;
  }

	if (s->connect(client_->addr(), client_->port())) {
		panic("Sock::connect() failed");
	}

	req = new request();
	if (!req) {
		return -ENOMEM;
  }

  if (clock_gettime(CLOCK_MONOTONIC, &req->start_ts) < 0) {
    perror("clock_gettime()");
    return -errno;
  }

	req->should_measure = should_measure;
	req->service_us = ceil(d(client_->get_randgen()));
	req->req.nr = 1;
	req->req.tag = (uint64_t) req;
	req->req.delays[0] = req->service_us;

	ent.buf = (char *) &req->req;
	ent.len = sizeof(struct req_pkt);
	ent.complete = NULL;
	if (s->write(&ent)) {
		fprintf(stderr, "ran out of tx buffers\n");
		return -ENOSPC;
	}

	ent.buf = (char *) &req->resp;
	ent.len = sizeof(struct resp_pkt);
	ent.data = (void *) &req->resp;
	ent.complete = &read_completion_handler;
	if (s->read(&ent)) {
		fprintf(stderr, "ran out of rx buffers\n");
		return -ENOSPC;
	}

	return 0;
}
