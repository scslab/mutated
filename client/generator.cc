/*
 * generator_reqperflow - creates a new TCP connection for each request
 */

#include <errno.h>

#include "common.hh"
#include "generator.hh"
#include "socket.hh"
#include "time.hh"
#include "protocol.hh"

int generator_flowperreq::start(void)
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

static void read_completion_handler(struct sock *s, void *data, int status)
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

	record_sample(service_us, wait_us, req->should_measure);

	delete req;
	socket_put(s);
}

int generator_flowperreq::do_request(bool should_measure)
{
	int ret;
	std::exponential_distribution<> d(1 / (double) cfg.service_us);
	struct sg_ent ent;
	struct request *req;
	struct sock *s = socket_alloc();
	if (!s)
		return -ENOMEM;

	ret = socket_create(s, cfg.addr, cfg.port);
	if (ret) {
		panic("socket_create() failed");
	}

	req = new request();
	if (!req)
		return -ENOMEM;

        ret = clock_gettime(CLOCK_MONOTONIC, &req->start_ts);
        if (ret == -1) {
                perror("clock_gettime()");
                return -errno;
        }

	req->should_measure = should_measure;
	req->service_us = ceil(d(randgen));
	req->req.nr = 1;
	req->req.tag = (uint64_t) req;
	req->req.delays[0] = req->service_us;

	ent.buf = (char *) &req->req;
	ent.len = sizeof(struct req_pkt);
	ent.complete = NULL;
	if (socket_write(s, &ent)) {
		fprintf(stderr, "ran out of tx buffers\n");
		return -ENOSPC;
	}

	ent.buf = (char *) &req->resp;
	ent.len = sizeof(struct resp_pkt);
	ent.data = (void *) &req->resp;
	ent.complete = &read_completion_handler;
	if (socket_read(s, &ent)) {
		fprintf(stderr, "ran out of rx buffers\n");
		return -ENOSPC;
	}

	return 0;
}
