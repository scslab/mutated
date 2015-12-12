#ifndef MUTATED_SOCKET_HH
#define MUTATED_SOCKET_HH

/**
 * socket.h - async socket I/O support
 */

#include <cstdint>
#include <cstring>

class Sock;

/* A vector IO operation (data segment) */
struct vio {
	char   *buf;
	size_t len;
	void   *cb_data;
	void   (*complete) (Sock *s, void *data, int ret);
};

/**
 * Asynchronous socket (TCP only).
 */
class Sock {
public:
	/* Maximum number of outstanding vector IO operations */
	static constexpr size_t MAX_SGS = 64;

private:
	int ref_cnt;              /* the reference count */
	int fd_;                   /* the file descriptor */
	unsigned short port;
	bool connected;           /* is the socket connected? */
	bool rx_rdy;              /* ready to read? */
	bool tx_rdy;              /* ready to write? */
	size_t rx_nrents;            /* number of RX SGs */
	size_t tx_nrents;            /* number of TX SGs */
	vio rx_ents[MAX_SGS];
	vio tx_ents[MAX_SGS];

	/* Low-level recv & send*/
	void rx(void);
	void tx(void);

public:
	/* Constructor */
	Sock(void);

	/* Deconstructor */
	~Sock(void);

	/* Disable copy and move */
	Sock(const Sock &) = delete;
	Sock(Sock &&) = delete;
	Sock operator=(const Sock &) = delete;
	Sock operator=(Sock &&) = delete;

	/* Access underlying file descriptor */
	int fd(void) const noexcept { return fd_; }

	/* Open a new remote connection */
	void connect(const char *addr, unsigned short port);

	/* Read and write (vector IO support) */
	void read(const vio & ent);
	void write(const vio & ent);

	/* Handle epoll events against this socket */
	void run_io(uint32_t events);

public:
	void get(void); /* Take a new reference */
	void put(void); /* Release a reference */
};

#endif /* MUTATED_SOCKET_HH */
