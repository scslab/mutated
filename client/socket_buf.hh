#ifndef MUTATED_SOCKET2_HH
#define MUTATED_SOCKET2_HH

/**
 * socket_buf.hh - async socket I/O support. A variant that uses circular
 * buffers internally for memory management of the rx and tx queues.
 */

#include <cstdint>
#include <cstring>
#include <functional>
#include <utility>

#include <limits.h>

#include "buffer.hh"

class Sock;

/* An IO operation */
struct ioop {
    using ioop_cb =
      std::function<void(Sock *, char *, size_t, char *, size_t, void *, int)>;

    size_t len;
    void *cb_data;
    ioop_cb cb;

    ioop(void)
      : len{0}
      , cb_data{nullptr}
      , cb{}
    {
    }

    ioop(size_t len_, void *data_, ioop_cb complete_)
      : len{len_}
      , cb_data{data_}
      , cb{complete_}
    {
    }

    ioop(const ioop &) = default;
    ioop &operator=(const ioop &) = default;
};

/**
 * Asynchronous socket (TCP only). Uses circular buffers internally for
 * managing rx and tx queues.
 */
class Sock
{
  public:
    /* Maximum number of outstanding read IO operations */
    using ioqueue = buffer<ioop, 1024>;

  private:
    int ref_cnt;         /* the reference count */
    int fd_;             /* the file descriptor */
    unsigned short port; /* port connected to */
    bool connected;      /* is the socket connected? */
    bool rx_rdy;         /* ready to read? */
    bool tx_rdy;         /* ready to write? */

    ioqueue rxcbs; /* read queue */
    charbuf rbuf;  /* read buffer */
    charbuf wbuf;  /* write buffer */

    void rx(void); /* receive handler */
    void tx(void); /* transmit handler */

  public:
    /* Constructor */
    Sock(void) noexcept;

    /* Deconstructor */
    ~Sock(void) noexcept;

    /* Disable copy and move */
    Sock(const Sock &) = delete;
    Sock(Sock &&) = delete;
    Sock operator=(const Sock &) = delete;
    Sock operator=(Sock &&) = delete;

    /* Take a new reference */
    void get(void) noexcept;
    /* Release a reference */
    void put(void);

    /* Access underlying file descriptor */
    int fd(void) const noexcept { return fd_; }

    /* Open a new remote connection */
    void connect(const char *addr, unsigned short port);

    /* Read queuing */
    void read(const ioop &);

    /* Write queuing */
    std::pair<char *, char *> write_prepare(size_t &len);
    void write_commit(const size_t len);

    /* Handle epoll events against this socket */
    void run_io(uint32_t events);
};

#endif /* MUTATED_SOCKET2_HH */
