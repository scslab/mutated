/**
 * socket.c - async socket I/O support
 */
#include <algorithm>
#include <system_error>
#include <utility>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "socket2.hh"
#include "util.hh"

using namespace std;

/**
 * Sock - construct a new socket.
 */
Sock::Sock(void) noexcept : ref_cnt{1},
                            fd_{-1},
                            port{0},
                            connected{false},
                            rx_rdy{false},
                            tx_rdy{false},
                            rxcbs{},
                            rbuf{},
                            wbuf{}
{
}

/**
 * ~Sock - deconstruct a socket.
 */
Sock::~Sock(void) noexcept
{
    // cancel all pending read requests
    for (auto &rxcb : rxcbs) {
        if (rxcb.cb) {
            rxcb.cb(this, nullptr, 0, nullptr, 0, rxcb.cb_data, -EIO);
        }
    }

    /* we don't check for any errors since difficult to handle in a
     * deconstructor */
    if (fd_ >= 0) {
        /* Explicitly send a FIN */
        shutdown(fd_, SHUT_RDWR);

        /* Turn on (zero-length) linger to avoid running out of ports by
         * sending a RST when we close the file descriptor.
         *
         * ezyang: see my writeup at http://stackoverflow.com/a/28377819/23845
         * for details.
         */
        linger linger;
        linger.l_onoff = 1;
        linger.l_linger = 0;
        setsockopt(fd_, SOL_SOCKET, SO_LINGER, (char *)&linger,
                   sizeof(linger));
        close(fd_);
    }
}

/**
 * get - increase the reference count.
 */
void Sock::get(void) noexcept { ref_cnt++; }

/**
 * set - decrease the reference count, will deallocate if hits zero.
 */
void Sock::put(void)
{
    if (--ref_cnt == 0) {
        delete this;
    } else if (ref_cnt < 0) {
        throw std::runtime_error("sock::put refcnt < 0");
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
    sockaddr_in saddr;

    port = portt;
    fd_ =
      SystemCall(socket(AF_INET, SOCK_STREAM, 0), "Sock::connect: socket()");

    /* make the socket nonblocking */
    opts = SystemCall(fcntl(fd_, F_GETFL), "Sock::connect: fcntl(F_GETFL)");
    opts = (opts | O_NONBLOCK);
    SystemCall(fcntl(fd_, F_SETFL, opts), "Sock::connect: fcntl(F_SETFL)");

    /* connect */
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = inet_addr(addr);
    saddr.sin_port = htons(port);

    ret = ::connect(fd_, (sockaddr *)&saddr, sizeof(saddr));
    if (ret == -1 and errno != EINPROGRESS) {
        throw system_error(errno, system_category(),
                           "Sock::connect: connect()");
    }

    /* disable TCP nagle algorithm (for lower latency) */
    opts = 1;
    SystemCall(
      setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, (char *)&opts, sizeof(int)),
      "Sock::connect: setsockopt(TCP_NODELAY)");
}

/**
 * rx - receive segments from the wire.
 */
void Sock::rx(void)
{
    // is anything pending for read?
    if (rxcbs.items() == 0) {
        return;
    }

    // do the read
    ssize_t nbytes;
    size_t n = rbuf.avail(), n1 = n;
    auto rptrs = rbuf.queue_prep(n1);
    if (rptrs.second == nullptr) {
        // no wrapping, normal read
        nbytes = ::read(fd_, rptrs.first, n);
    } else {
        // need to wrap, so use writev
        iovec iov[2];
        iov[0].iov_base = rptrs.first;
        iov[0].iov_len = n1;
        iov[1].iov_base = rptrs.second;
        iov[1].iov_len = n - n1;
        nbytes = ::readv(fd_, iov, 2);
    }

    if (nbytes < 0 and errno == EAGAIN) {
        rx_rdy = false;
        return;
    } else if (nbytes <= 0) {
        throw system_error(errno, system_category(), "Sock::rx: readv error");
    } else if (size_t(nbytes) > n) {
        throw runtime_error("Sock::rx: read returned more bytes than asked");
    }
    rbuf.queue_commit(nbytes);

    get();
    for (auto &rxcb : rxcbs) {
        if (rbuf.items() < rxcb.len) {
            break;
        }
        if (rxcb.cb) {
            n = n1 = rxcb.len;
            rptrs = rbuf.peek(n1);
            rxcb.cb(this, rptrs.first, n1, rptrs.second, n - n1, rxcb.cb_data,
                    0);
        }
        rbuf.drop(rxcb.len);
        rxcbs.drop(1);
    }
    put();
}

/**
 * read - enqueue data to receive from the socket and read if socket ready.
 * @ent: the scatter-gather entry
 */
void Sock::read(const ioop &op)
{
    size_t n = 1;
    *rxcbs.queue(n) = op;
    if (rx_rdy) {
        rx();
    }
}

/**
 * tx - push pending writes to the wire.
 */
void Sock::tx(void)
{
    /* is anything pending for send? */
    if (wbuf.items() == 0) {
        return;
    }

    ssize_t nbytes;
    size_t n = wbuf.items(), n1 = n;
    auto wptrs = wbuf.peek(n1);

    if (wptrs.second == nullptr) {
        /* no wrapping, normal write */
        nbytes = ::write(fd_, wptrs.first, n);
    } else {
        /* need to wrap, so use writev */
        iovec iov[2];
        iov[0].iov_base = wptrs.first;
        iov[0].iov_len = n1;
        iov[1].iov_base = wptrs.second;
        iov[1].iov_len = n - n1;
        nbytes = ::writev(fd_, iov, 2);
    }

    if (nbytes < 0) {
        if (errno == EAGAIN) {
            tx_rdy = false;
            return;
        } else {
            throw system_error(errno, system_category(),
                               "Sock::tx: write error");
        }
    } else if (size_t(nbytes) > n) {
        throw runtime_error("Sock::tx: write sent more bytes than asked");
    }
    wbuf.drop(nbytes);
}

/**
 * Prepare a write on this socket.
 * @len: the size of the requested write. The available buffer through the
 * first pointer is returned through len.
 * @return: the buffer, split between two contiguous segments, for the write.
 * The length of the first segment is returned through len, the length of the
 * second segment is the remaining needed buffer to fullfill the request.
 */
pair<char *, char *> Sock::write(size_t &len)
{
    size_t fulllen = len;
    auto wptrs = wbuf.queue_prep(len);
    wbuf.queue_commit(fulllen);
    return wptrs;
}

/**
 * __socket_check_connected - check if their is a socket error on the file.
 * @fd: the file descriptor to check for a socket error
 * @return: throws the error if present.
 */
static void __socket_check_connected(int fd)
{
    int valopt;
    socklen_t len = sizeof(int);

    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)&valopt, &len)) {
        throw system_error(errno, system_category(),
                           "Sock::__socket_get_connect_error: getsockopt()");
    } else if (valopt) {
        throw system_error(valopt, system_category(),
                           "Sock::run_io: socket failed to connect");
    }
}

/**
 * run_io - handle an epoll event against the socket.
 * @events: the bitmask of epoll events that occured.
 */
void Sock::run_io(uint32_t events)
{
    get();

    if (events & EPOLLIN) {
        rx_rdy = true;
        rx();
    }

    if (events & EPOLLOUT) {
        if (not connected) {
            __socket_check_connected(fd_);
            connected = true;
        }
        tx_rdy = true;
        tx();
    }

    put();
}
