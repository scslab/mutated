#ifndef MUTATED_SOCKET_BUF_HH
#define MUTATED_SOCKET_BUF_HH

/**
 * socket_buf.hh - async socket I/O support. A variant that uses circular
 * buffers internally for memory management of the rx and tx queues.
 */

// XXX: If we allocate a Socket on the stack it segfaults due to the buffer
// sizes.

#include <cstdint>
#include <cstring>
#include <functional>
#include <utility>

#include "buffer.hh"
#include "limits.hh"

class Sock;

/**
 * A RX IO operation.
 */
class ioop_rx
{
  public:
    using ioop_cb = std::function<size_t(Sock *, void *, char *, size_t,
                                         char *, size_t, int)>;

    size_t hdrlen;
    ioop_cb hdrcb;
    size_t bodylen;
    ioop_cb bodycb;
    void *cbdata;

    ioop_rx(void) noexcept : hdrlen{0},
                             hdrcb{},
                             bodylen{0},
                             bodycb{},
                             cbdata{nullptr}
    {
    }

    ioop_rx(size_t hdrlen_, ioop_cb hdrcb_, size_t bodylen_, ioop_cb bodycb_,
            void *cbdata_) noexcept : hdrlen{hdrlen_},
                                      hdrcb{hdrcb_},
                                      bodylen{bodylen_},
                                      bodycb{bodycb_},
                                      cbdata{cbdata_}
    {
    }

    ioop_rx(const ioop_rx &) = default;
    ioop_rx &operator=(const ioop_rx &) = default;
    ~ioop_rx(void) noexcept {}
};

/**
 * A TX IO operation.
 */
class ioop_tx
{
  public:
    using ioop_cb = std::function<void(Sock *, void *, int)>;

    size_t len;
    ioop_cb cb;
    void *cbdata;

    ioop_tx(void) noexcept : len{0}, cb{}, cbdata{nullptr} {}

    ioop_tx(size_t len_, ioop_cb cb_, void *cbdata_) noexcept : len{len_},
                                                                cb{cb_},
                                                                cbdata{cbdata_}
    {
    }

    ioop_tx(const ioop_tx &) = default;
    ioop_tx &operator=(const ioop_tx &) = default;
    ~ioop_tx(void) noexcept {}
};

/**
 * Asynchronous socket (TCP only). Uses circular buffers internally for
 * managing rx and tx queues.
 *
 * NOTE: write operations (write_commit, write, write_emplace) should be
 * followed by a try_tx to try sending the data if the socket is ready. Write
 * operations place data on the tx buffer but don't attempt to transmit.
 */
class Sock
{
  public:
    using rxqueue = buffer<ioop_rx, MAX_OUTSTANDING_REQS>;
    using txqueue = buffer<ioop_tx, MAX_OUTSTANDING_REQS>;

  private:
    int fd_;             /* the file descriptor */
    unsigned short port; /* port connected to */
    bool connected;      /* is the socket connected? */
    bool rx_rdy;         /* ready to read? */
    bool tx_rdy;         /* ready to write? */

    rxqueue rxcbs; /* read queue */
    charbuf rbuf;  /* read buffer */

    txqueue txcbs; /* write queue */
    charbuf wbuf;  /* write buffer */
    size_t txout;  /* total tx data waiting to be sent in txcbs queue */

    void rx(void); /* receive handler */
    void tx(void); /* transmit handler */

  public:
    Sock(void) noexcept;
    ~Sock(void) noexcept;

    /* Disable copy and move */
    Sock(const Sock &) = delete;
    Sock(Sock &&) = delete;
    Sock operator=(const Sock &) = delete;
    Sock operator=(Sock &&) = delete;

    /* Access underlying file descriptor */
    int fd(void) const noexcept { return fd_; }

    /* Open a new remote connection */
    void connect(const char *addr, unsigned short port);

    /* Read queueing */
    void read(const ioop_rx &io);

    /* Write queueing preparation */
    std::pair<char *, char *> write_prepare(size_t &len);
    void write_commit(const size_t len);

    /* Write */
    void write(const void *data, const size_t len);

    /* Write by constructing in-place */
    template <class T, class... Args> void write_emplace(Args &&... args)
    {
        std::size_t n = sizeof(T), n1 = n;
        auto p = write_prepare(n1);
        if (p.second == nullptr) {
            ::new (static_cast<void *>(p.first))
              T(std::forward<Args>(args)...);
        } else {
            T t(std::forward<Args>(args)...);
            memcpy(p.first, &t, n1);
            memcpy(p.second, &t + n1, n - n1);
        }
        write_commit(n);
    }

    /* Insert a write callback to fire once all data previously inserted into
     * the queue has been sent. You should insert a callback point _before_
     * calling `try_tx()` ideally. */
    void write_cb_point(const ioop_tx::ioop_cb cb, void *data);

    /* Attempt to transmit data if the socket is ready */
    void try_tx(void);

    /* Handle epoll events against this socket */
    void run_io(uint32_t events);
};

#endif /* MUTATED_SOCKET_BUF_HH */
