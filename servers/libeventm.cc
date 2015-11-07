#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cerrno>

#include <arpa/inet.h>
#include <netinet/tcp.h>

#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include "protocol.hh"
#include "workload.hh"
#include "server_common.hh"

static void
echo_read_cb(struct bufferevent *bev, void *ctx)
{
	struct evbuffer *input = bufferevent_get_input(bev);
	struct evbuffer *output = bufferevent_get_output(bev);
	struct req_pkt req;
	struct resp_pkt resp;
	struct workload *w;

	if (evbuffer_get_length(input) < sizeof(struct req_pkt))
		return;

	evbuffer_remove(input, &req, sizeof(req));

	w = workload_alloc();
	if (!w)
		exit(1);
	workload_run(w, req.delays[0]);
	free(w);
	resp.tag = req.tag;

	evbuffer_add(output, &resp, sizeof(resp));
}

static void
echo_event_cb(struct bufferevent *bev, short events, void *ctx)
{
	if (events & BEV_EVENT_ERROR)
		perror("Error from bufferevent");
	if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
		bufferevent_free(bev);
	}
}

static void
accept_conn_cb(struct evconnlistener *listener,
    evutil_socket_t fd, struct sockaddr *address, int socklen,
    void *ctx)
{
	int ret, opts = 1;
	struct event_base *base = evconnlistener_get_base(listener);
	struct bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);

	ret = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
			 (char *) &opts, sizeof(int));
	if (ret == -1) {
		perror("setsockopt()");
		exit(1);
	}

	bufferevent_setcb(bev, echo_read_cb, NULL, echo_event_cb, NULL);
	bufferevent_enable(bev, EV_READ|EV_WRITE);
}

static void
accept_error_cb(struct evconnlistener *listener, void *ctx)
{
	struct event_base *base = evconnlistener_get_base(listener);
	int err = EVUTIL_SOCKET_ERROR();

	fprintf(stderr, "Got an error %d (%s) on the listener. "
		"Shutting down.\n", err, evutil_socket_error_to_string(err));

	event_base_loopexit(base, NULL);
}

static void *worker_thread(void *arg)
{
	struct event_base *base;
	struct evconnlistener *listener;
	struct sockaddr_in sin;

	int cpu = (long) arg;
	set_affinity(cpu);

	base = event_base_new();
	if (!base) {
		puts("Couldn't open event base");
		exit(1);
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(0);
	sin.sin_port = htons(8080);

	listener = evconnlistener_new_bind(base, accept_conn_cb, NULL,
					   LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE | LEV_OPT_REUSEABLE_PORT, -1,
					   (struct sockaddr*) &sin, sizeof(sin));
	if (!listener) {
		perror("Couldn't create listener");
		exit(1);
	}
	evconnlistener_set_error_cb(listener, accept_error_cb);

	event_base_dispatch(base);

	return NULL;
}

int
main(int argc, char **argv)
{
	workload_setup(100);
	create_worker_per_core(worker_thread, false);
	return 0;
}
