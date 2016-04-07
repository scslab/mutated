#ifndef MUTATED_SOCKET2_HH
#define MUTATED_SOCKET2_HH

/**
 * socket2.h - async socket I/O support
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

    size_t len = 0;
    void *cb_data = nullptr;
    ioop_cb cb = nullptr;
};

/**
 * Asynchronous socket (TCP only).
 */
class Sock
{
  public:
    /* read buffer size */
    static constexpr size_t RBUF_SIZ = 1024 * 1024;
    /* write buffer size */
    static constexpr size_t WBUF_SIZ = 1024 * 1024;

    /* Maximum number of outstanding vector IO operations */
    using ioqueue = buffer<ioop, IOV_MAX>;

  private:
    int ref_cnt;         /* the reference count */
    int fd_;             /* the file descriptor */
    unsigned short port; /* port connected to */
    bool connected;      /* is the socket connected? */
    bool rx_rdy;         /* ready to read? */
    bool tx_rdy;         /* ready to write? */

    ioqueue rxcbs; /* ioops queue */
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
    std::pair<char *, char *> write(size_t &len);

    /* Handle epoll events against this socket */
    void run_io(uint32_t events);
};

#endif /* MUTATED_SOCKET2_HH */
