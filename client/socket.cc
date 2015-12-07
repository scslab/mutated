/**
 * socket.c - async socket I/O support
 */

#include <alloca.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "client.hh"
#include "debug.hh"
#include "socket.hh"

/**
 * Sock -- construct a new socket.
 */
Sock::Sock(void)
	: ref_cnt{1}, fd{0}, port{0}, connected{false}, rx_rdy{false}, tx_rdy{false}
	, rx_nrents{0}, tx_nrents{0}
{
}

/**
 * ~Sock -- deconstruct a socket.
 */
Sock::~Sock(void)
{
	for (int i = 0; i < rx_nrents; i++) {
		if (rx_ents[i].complete) {
			rx_ents[i].complete(this, rx_ents[i].data, -EIO);
		}
	}

	for (int i = 0; i < tx_nrents; i++) {
		if (tx_ents[i].complete) {
			tx_ents[i].complete(this, tx_ents[i].data, -EIO);
		}
	}

	/* Explicitly send a FIN */
	shutdown(fd, SHUT_RDWR);

	/* Turn on (zero-length) linger to avoid running out of ports by
	 * sending a RST when we close the file descriptor.
	 *
	 * ezyang: see my writeup at http://stackoverflow.com/a/28377819/23845 for
	 * details.
	 */
	struct linger linger;
	linger.l_onoff = 1;
	linger.l_linger = 0;
	int ret = setsockopt(fd, SOL_SOCKET, SO_LINGER,
		(char *) &linger, sizeof(linger));
	if (ret == -1) {
		panic("__socket_free: SO_LINGER failed");
	}

	close(fd);
}

/**
 * get -- increase the reference count.
 */
void Sock::get(void)
{
	ref_cnt++;
}

/**
 * set -- decrease the reference count, will deallocate if hits zero.
 */
void Sock::put(void)
{
	if (!(--ref_cnt)) {
    /* TODO: Not clear this is a good approach */
    delete this;
	}
}

/**
 * connect - establishes an outgoing TCP connection.
 * @addr: the IPv4 address
 * @port: the destination port
 * @return: 0 if successful, otherwise < 0
 *
 * NOTE: disables nagle and makes the socket nonblocking.
 */
int Sock::connect(const char *addr, unsigned short portt)
{
	int fdd, ret, opts;
	struct sockaddr_in saddr;

	fdd = socket(AF_INET, SOCK_STREAM, 0);
	if (fdd == -1) {
		perror("socket()");
		return -errno;
	}

	/* make the socket nonblocking */
	if ((opts = fcntl(fdd, F_GETFL)) < 0) {
		perror("fcntl(F_GETFL)");
    close(fdd);
    return -1;
	}
	opts = (opts | O_NONBLOCK);
	if (fcntl(fdd, F_SETFL, opts) < 0) {
		perror("fcntl(F_SETFL)");
    close(fdd);
    return -1;
	}

	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = inet_addr(addr);
	saddr.sin_port = htons(portt);

	ret = ::connect(fdd, (struct sockaddr *) &saddr, sizeof(saddr));
	if (ret == -1 && errno != EINPROGRESS) {
		perror("connect()");
		close(fdd);
		return -1;
	}

	/* disable TCP nagle algorithm (for lower latency) */
	opts = 1;
	ret = setsockopt(fdd, IPPROTO_TCP, TCP_NODELAY, (char *) &opts, sizeof(int));
	if (ret == -1) {
		perror("setsockopt(TCP_NODELAY)");
		close(fdd);
		return -1;
	}

	/* register for epoll events on this socket */
	if (client_->epoll_watch(fdd, this, EPOLLIN | EPOLLOUT)) {
		perror("epoll_watch");
		close(fdd);
		return -1;
	}

	fd = fdd;
	port = portt;

	return 0;
}

/**
 * rx - receive segments from the wire.
 * @return: 0 if successful, otherwise fail
 */
int Sock::rx(void)
{
	int i, j;
	ssize_t ret;
	struct iovec *iov = (struct iovec *) alloca(sizeof(struct iovec) * tx_nrents);

	/* is anything pending for read? */
	if (!rx_nrents) {
		return 0;
	}

	for (i = 0; i < rx_nrents; i++) {
		iov[i].iov_base = rx_ents[i].buf;
		iov[i].iov_len = rx_ents[i].len;
	}

	ret = readv(fd, iov, rx_nrents);
	if (ret < 0 && errno == EINTR) {
		/* this can't happen unless signals get misconfigured */
		panic("sock::rx: readv interrupted");
	}
	if (ret < 0 && errno == EAGAIN) {
		rx_rdy = false;
		return 0;
	}
	if (ret <= 0)
		return -EHOSTDOWN;

	get();
	for (i = 0; i < rx_nrents; i++) {
		if ((size_t) ret < rx_ents[i].len) {
			rx_ents[i].len -= ret;
			rx_ents[i].buf += ret;

			for (j = i; j < rx_nrents; j++)
				rx_ents[j - i] = rx_ents[j];

			rx_nrents -= i;
			put();
			return 0;
		}

		if (rx_ents[i].complete)
			rx_ents[i].complete(this, rx_ents[i].data, 0);
		ret -= rx_ents[i].len;
	}

	if (ret != 0) {
		panic("sock::rx: readv() returned more bytes than asked\n");
	}

	rx_nrents = 0;
	put();
	return 0;
}

/**
 * read - enqueue data to receive from the socket.
 * @ent: the scatter-gather entry
 * @return: 0 if sucessful, otherwise fail
 */
int Sock::read(struct sg_ent *ent)
{
	if (rx_nrents >= MAX_SGS) {
		return -ENOSPC;
	}

	rx_ents[rx_nrents++] = *ent;

	if (!rx_rdy) {
		return 0;
	}

	return rx();
}

/**
 * tx - push pending writes to the wire.
 * @s: the socket
 * @return: 0 if successful, otherwise fail
 */
int Sock::tx(void)
{
	int i, j;
	ssize_t ret;
	struct iovec *iov = (struct iovec *) alloca(sizeof(struct iovec) * tx_nrents);

	/* is anything pending for send? */
	if (!tx_nrents)
		return 0;

	for (i = 0; i < tx_nrents; i++) {
		iov[i].iov_base = tx_ents[i].buf;
		iov[i].iov_len = tx_ents[i].len;
	}

	ret = writev(fd, iov, tx_nrents);
	if (ret < 0 && errno == EINTR) {
		/* this can't happen unless signals get misconfigured */
		panic("socket_tx: writev() interrupted");
	}
	if (ret < 0 && errno == EAGAIN) {
		tx_rdy = false;
		return 0;
	}
	if (ret < 0)
		return -EHOSTDOWN;

	get();
	for (i = 0; i < tx_nrents; i++) {
		if ((size_t) ret < tx_ents[i].len) {
			tx_ents[i].len -= ret;
			tx_ents[i].buf += ret;

			for (j = i; j < tx_nrents; j++)
				tx_ents[j - i] = tx_ents[j];

			tx_nrents -= i;
			put();
			return 0;
		}

		if (tx_ents[i].complete)
			tx_ents[i].complete(this, tx_ents[i].data, 0);
		ret -= tx_ents[i].len;
	}

	if (ret != 0) {
		panic("writev() returned more bytes than asked");
	}

	tx_nrents = 0;
	put();
	return 0;
}

/**
 * write - enqueue data to send on the socket.
 * @ent: the scatter-gather entry
 * @return: 0 if successful, otherwise fail
 */
int Sock::write(struct sg_ent *ent)
{
	if (tx_nrents >= MAX_SGS) {
		return -ENOSPC;
	}

	tx_ents[tx_nrents++] = *ent;

	if (!tx_rdy) {
		return 0;
	}

	return tx();
}

static int __socket_get_connect_error(int fd)
{
	int valopt;
	socklen_t len = sizeof(int);

	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *) &valopt, &len)) {
		perror("getsockopt()");
		return -errno;
	}

	return -valopt;
}

void Sock::handler(uint32_t events)
{
	int ret;

	get();
	if (events & EPOLLIN) {
		rx_rdy = true;
		ret = rx();
		if (ret) {
			panic("Sock::handler: Sock::rx() failed");
		}
	}

	if (events & EPOLLOUT) {
		if (!connected) {
			ret = __socket_get_connect_error(fd);
			if (ret) {
				panic("Sock::handler: socket failed to connect, ret = %d\n", ret);
			}
			connected = true;
		}

		tx_rdy = true;
		ret = tx();
		if (ret) {
			panic("Sock::handler: Sock::tx() failed");
		}
	}
	put();
}
