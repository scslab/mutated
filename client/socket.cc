/**
 * socket.c - async socket I/O support
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <alloca.h>
#include <unistd.h>

#include "common.hh"
#include "socket.hh"

extern int epoll_watch(int fd, void *data, uint32_t events);

void socket_get(struct sock *s)
{
	s->ref_cnt++;
}

static void __socket_free(struct sock *s)
{
	int i;

	for (i = 0; i < s->rx_nrents; i++) {
		if (s->rx_ents[i].complete)
			s->rx_ents[i].complete(s, s->rx_ents[i].data, -EIO);
	}

	for (i = 0; i < s->tx_nrents; i++) {
		if (s->tx_ents[i].complete)
			s->tx_ents[i].complete(s, s->tx_ents[i].data, -EIO);
	}

	// Explicitly send a FIN
	shutdown(s->fd, SHUT_RDWR);

	/* Turn on (zero-length) linger to avoid running out of ports by
	 * sending a RST when we close the file descriptor.
	 *
	 * ezyang: see my writeup at http://stackoverflow.com/a/28377819/23845
	 * for details.
	 */
	struct linger linger;
	linger.l_onoff = 1;
	linger.l_linger = 0;
	int ret = setsockopt(s->fd, SOL_SOCKET, SO_LINGER,
			 (char *) &linger, sizeof(linger));
	if (ret == -1) {
		panic("__socket_free: SO_LINGER failed");
	}

	close(s->fd);
	free(s);
}

void socket_put(struct sock *s)
{
	if (!(--s->ref_cnt))
		__socket_free(s);
}

static int socket_rx(struct sock *s)
{
	int i, j;
	ssize_t ret;
	struct iovec *iov = (struct iovec *) alloca(sizeof(struct iovec) * s->tx_nrents);

	/* is anything pending for read? */
	if (!s->rx_nrents)
		return 0;

	for (i = 0; i < s->rx_nrents; i++) {
		iov[i].iov_base = s->rx_ents[i].buf;
		iov[i].iov_len = s->rx_ents[i].len;
	}

	ret = readv(s->fd, iov, s->rx_nrents);
	if (ret < 0 && errno == EINTR) {
		/* this can't happen unless signals get misconfigured */
		panic("socket_rx: readv interrupted");
	}
	if (ret < 0 && errno == EAGAIN) {
		s->rx_rdy = 0;
		return 0;
	}
	if (ret <= 0)
		return -EHOSTDOWN;

	socket_get(s);
	for (i = 0; i < s->rx_nrents; i++) {
		if ((size_t) ret < s->rx_ents[i].len) {
			s->rx_ents[i].len -= ret;
			s->rx_ents[i].buf += ret;

			for (j = i; j < s->rx_nrents; j++)
				s->rx_ents[j - i] = s->rx_ents[j];

			s->rx_nrents -= i;
			socket_put(s);
			return 0;
		}

		if (s->rx_ents[i].complete)
			s->rx_ents[i].complete(s, s->rx_ents[i].data, 0);
		ret -= s->rx_ents[i].len;
	}

	if (ret != 0) {
		panic("socket_rx: readv() returned more bytes than asked\n");
	}

	s->rx_nrents = 0;
	socket_put(s);
	return 0;
}

/**
 * socket_tx - push pending writes to the wire
 * @s: the socket
 *
 * Returns 0 if successful, otherwise fail.
 */
static int socket_tx(struct sock *s)
{
	int i, j;
	ssize_t ret;
	struct iovec *iov = (struct iovec *) alloca(sizeof(struct iovec) * s->tx_nrents);

	/* is anything pending for send? */
	if (!s->tx_nrents)
		return 0;

	for (i = 0; i < s->tx_nrents; i++) {
		iov[i].iov_base = s->tx_ents[i].buf;
		iov[i].iov_len = s->tx_ents[i].len;
	}

	ret = writev(s->fd, iov, s->tx_nrents);
	if (ret < 0 && errno == EINTR) {
		/* this can't happen unless signals get misconfigured */
		panic("socket_tx: writev() interrupted");
	}
	if (ret < 0 && errno == EAGAIN) {
		s->tx_rdy = 0;
		return 0;
	}
	if (ret < 0)
		return -EHOSTDOWN;

	socket_get(s);
	for (i = 0; i < s->tx_nrents; i++) {
		if ((size_t) ret < s->tx_ents[i].len) {
			s->tx_ents[i].len -= ret;
			s->tx_ents[i].buf += ret;

			for (j = i; j < s->tx_nrents; j++)
				s->tx_ents[j - i] = s->tx_ents[j];

			s->tx_nrents -= i;
			socket_put(s);
			return 0;
		}

		if (s->tx_ents[i].complete)
			s->tx_ents[i].complete(s, s->tx_ents[i].data, 0);
		ret -= s->tx_ents[i].len;
	}

	if (ret != 0) {
		panic("writev() returned more bytes than asked");
	}

	s->tx_nrents = 0;
	socket_put(s);
	return 0;
}

/**
 * socket_read - enqueue data to receive from the socket
 * @s: the socket
 * @ent: the scatter-gather entry
 *
 * Returns 0 if sucessful, otherwise fail.
 */
int socket_read(struct sock *s, struct sg_ent *ent)
{
	if (s->rx_nrents >= MAX_SGS)
		return -ENOSPC;

	s->rx_ents[s->rx_nrents++] = *ent;

	if (!s->rx_rdy)
		return 0;

	return socket_rx(s);
}

/**
 * socket_write - enqueue data to send on the socket
 * @s: the socket
 * @ent: the scatter-gather entry
 *
 * Returns 0 if successful, otherwise fail.
 */
int socket_write(struct sock *s, struct sg_ent *ent)
{
	if (s->tx_nrents >= MAX_SGS)
		return -ENOSPC;

	s->tx_ents[s->tx_nrents++] = *ent;

	if (!s->tx_rdy)
		return 0;

	return socket_tx(s);
}

static int __socket_get_connect_error(struct sock *s)
{
	int valopt;
	socklen_t len = sizeof(int);

	if (getsockopt(s->fd, SOL_SOCKET, SO_ERROR, (void *) &valopt, &len)) {
		perror("getsockopt()");
		return -errno;
	}

	return -valopt;
}

/**
 * socket_create - creates an outgoing TCP connection
 * @s: the socket struct
 * @addr: the IPv4 address
 * @port: the destination port
 *
 * Returns 0 if successful, otherwise < 0.
 *
 * NOTE: disables nagle and makes the socket nonblocking
 */
int socket_create(struct sock *s, const char *addr, unsigned short port)
{
	int fd, ret, opts;
	struct sockaddr_in saddr;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
		perror("socket()");
		return -errno;
	}

	/* make the socket nonblocking */
	opts = fcntl(fd, F_GETFL);
	if (opts < 0) {
		perror("fcntl(F_GETFL)");
		goto err;
	}
	opts = (opts | O_NONBLOCK);
	if (fcntl(fd, F_SETFL, opts) < 0) {
		perror("fcntl(F_SETFL)");
		goto err;
	}

	bzero(&saddr, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = inet_addr(addr);
	saddr.sin_port = htons(port);

	ret = connect(fd, (struct sockaddr *) &saddr, sizeof(saddr));
	if (ret == -1 && errno != EINPROGRESS) {
		perror("connect()");
		goto err;
	}

	/* disable TCP nagle algorithm (for lower latency) */
	opts = 1;
        ret = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
                         (char *) &opts, sizeof(int));
        if (ret == -1) {
                perror("setsockopt(TCP_NODELAY)");
                goto err;
        }

	/* register for epoll events on this socket */
	ret = epoll_watch(fd, s, EPOLLIN | EPOLLOUT);
	if (ret < 0) {
		goto err;
	}

	s->fd = fd;
	s->port = port;

	return 0;

err:
	close(fd);
	return -errno;
}

struct sock *socket_alloc(void)
{
	struct sock *s = (struct sock *) malloc(sizeof(*s));
	memset(s, 0, sizeof(*s));
	s->ref_cnt = 1;

	return s;
}

void socket_handler(struct sock *s, uint32_t events)
{
	int ret;

	socket_get(s);
	if (events & EPOLLIN) {
		s->rx_rdy = 1;
		ret = socket_rx(s);
		if (ret)
			panic("socket_handler: socket_rx() failed");
	}

	if (events & EPOLLOUT) {
		if (!s->connected) {
			ret = __socket_get_connect_error(s);
			if (ret) {
				panic("socket_handler: socket failed to connect, ret = %d\n", ret);
			}

			s->connected = 1;
		}

		s->tx_rdy = 1;
		ret = socket_tx(s);
		if (ret)
			panic("socket_handler: socket_tx() failed");
	}
	socket_put(s);
}
