/**
 * socket.c - async socket I/O support
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <system_error>

#include <alloca.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "client.hh"
#include "socket.hh"
#include "utils.hh"

using namespace std;

/**
 * Sock -- construct a new socket.
 */
Sock::Sock(void)
	: ref_cnt{1}, fd{-1}, port{0}, connected{false}, rx_rdy{false}, tx_rdy{false}
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

	if (fd >= 0) {
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
		SystemCall(
			setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *) &linger, sizeof(linger)),
			"Sock::~Sock: SO_LINGER failed");

		close(fd);
	}
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
	if (not --ref_cnt) {
		/* TODO: Not clear this is a good approach */
		delete this;
	}
}

/**
 * connect - establishes an outgoing TCP connection.
 * @addr: the IPv4 address
 * @port: the destination port
 *
 * NOTE: disables nagle and makes the socket nonblocking.
 */
void Sock::connect(const char *addr, unsigned short portt)
{
	int ret, opts;
	struct sockaddr_in saddr;

	port = portt;
	fd = SystemCall(
		socket(AF_INET, SOCK_STREAM, 0),
		"Sock::connect: socket()");

	/* make the socket nonblocking */
	opts = SystemCall(
		fcntl(fd, F_GETFL),
		"Sock::connect: fcntl(F_GETFL)");
	opts = (opts | O_NONBLOCK);
	SystemCall(
		fcntl(fd, F_SETFL, opts),
		"Sock::connect: fcntl(F_SETFL)");

	/* connect */
	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = inet_addr(addr);
	saddr.sin_port = htons(port);

	ret = ::connect(fd, (struct sockaddr *) &saddr, sizeof(saddr));
	if (ret == -1 and errno != EINPROGRESS) {
		throw system_error(errno, system_category(),
			"Sock::connect: connect()");
	}

	/* disable TCP nagle algorithm (for lower latency) */
	opts = 1;
	SystemCall(
		setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &opts, sizeof(int)),
		"Sock::connect: setsockopt(TCP_NODELAY)");

	/* register for epoll events on this socket */
	if (client_->epoll_watch(fd, this, EPOLLIN | EPOLLOUT)) {
		throw system_error(errno, system_category(),
			"Sock::connect: epoll_watch");
	}
}

/**
 * rx - receive segments from the wire.
 * @return: 0 if successful, otherwise fail
 */
void Sock::rx(void)
{
	int i, j;
	ssize_t ret;
	struct iovec *iov = (struct iovec *) alloca(sizeof(struct iovec) * tx_nrents);

	/* is anything pending for read? */
	if (not rx_nrents) {
		return;
	}

	for (i = 0; i < rx_nrents; i++) {
		iov[i].iov_base = rx_ents[i].buf;
		iov[i].iov_len = rx_ents[i].len;
	}

	ret = readv(fd, iov, rx_nrents);
	if (ret < 0 and errno == EAGAIN) {
		rx_rdy = false;
		return;
	} else if (ret <= 0) {
		throw system_error(errno, system_category(),
			"Sock::rx: readv error");
	}

	get();
	for (i = 0; i < rx_nrents; i++) {
		if ((size_t) ret < rx_ents[i].len) {
			rx_ents[i].len -= ret;
			rx_ents[i].buf += ret;

			for (j = i; j < rx_nrents; j++) {
				rx_ents[j - i] = rx_ents[j];
			}

			rx_nrents -= i;
			put();

			return;
		} else {
			if (rx_ents[i].complete) {
				rx_ents[i].complete(this, rx_ents[i].data, 0);
			}
			ret -= rx_ents[i].len;
		}
	}

	if (ret != 0) {
		throw runtime_error("Sock::rx: readv returned more bytes than asked");
	}

	rx_nrents = 0;
	put();
}

/**
 * read - enqueue data to receive from the socket.
 * @ent: the scatter-gather entry
 * @return: 0 if sucessful, otherwise fail
 */
void Sock::read(struct sg_ent *ent)
{
	if (rx_nrents >= MAX_SGS) {
		throw system_error(ENOSPC, system_category(),
			"Sock::read: too many segments");
	}

	rx_ents[rx_nrents++] = *ent;
	if (rx_rdy) {
		rx();
	}
}

/**
 * tx - push pending writes to the wire.
 * @s: the socket
 * @return: 0 if successful, otherwise fail
 */
void Sock::tx(void)
{
	int i, j;
	ssize_t ret;
	struct iovec *iov = (struct iovec *) alloca(sizeof(struct iovec) * tx_nrents);

	/* is anything pending for send? */
	if (not tx_nrents) {
		return;
	}

	for (i = 0; i < tx_nrents; i++) {
		iov[i].iov_base = tx_ents[i].buf;
		iov[i].iov_len = tx_ents[i].len;
	}

	ret = writev(fd, iov, tx_nrents);
	if (ret < 0 and errno == EAGAIN) {
		tx_rdy = false;
		return;
	} else if (ret < 0) {
		throw system_error(errno, system_category(),
			"Sock::tx: writev error");
	}

	get();
	for (i = 0; i < tx_nrents; i++) {
		if ((size_t) ret < tx_ents[i].len) {
			tx_ents[i].len -= ret;
			tx_ents[i].buf += ret;

			for (j = i; j < tx_nrents; j++) {
				tx_ents[j - i] = tx_ents[j];
			}

			tx_nrents -= i;
			put();

			return;
		} else {
			if (tx_ents[i].complete) {
				tx_ents[i].complete(this, tx_ents[i].data, 0);
			}
			ret -= tx_ents[i].len;
		}
	}

	if (ret != 0) {
		throw runtime_error("Sock::tx: writev returned more bytes than asked");
	}

	tx_nrents = 0;
	put();
}

/**
 * write - enqueue data to send on the socket.
 * @ent: the scatter-gather entry
 * @return: 0 if successful, otherwise fail
 */
void Sock::write(struct sg_ent *ent)
{
	if (tx_nrents >= MAX_SGS) {
		throw system_error(ENOSPC, system_category(),
			"Sock::write: too many segments");
	}

	tx_ents[tx_nrents++] = *ent;
	if (tx_rdy) {
		tx();
	}
}

static void __socket_check_connected(int fd)
{
	int valopt;
	socklen_t len = sizeof(int);

	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *) &valopt, &len)) {
		throw system_error(errno, system_category(),
			"Sock::__socket_get_connect_error: getsockopt()");
	} else if (valopt) {
		throw system_error(valopt, system_category(),
			"Sock::handler: socket failed to connect");
	}
}

void Sock::handler(uint32_t events)
{
	get();

	if (events & EPOLLIN) {
		rx_rdy = true;
		rx();
	}

	if (events & EPOLLOUT) {
		if (not connected) {
			__socket_check_connected(fd);
			connected = true;
		}
		tx_rdy = true;
		tx();
	}

	put();
}
