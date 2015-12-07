#ifndef SOCKET_HH
#define SOCKET_HH

/**
 * socket.h - async socket I/O support
 */

#include <cstdint>
#include <cstring>

class Sock;

/* A vector IO operation (data segment) */
struct sg_ent {
	char   *buf;
	size_t len;
	void   *data;
	void   (*complete) (Sock *s, void *data, int ret);
};

/* Maximum number of outstanding vector IO operations */
#define MAX_SGS 64

/**
 * Asynchronous socket (TCP only).
 */
class Sock {
private:
	int ref_cnt;              /* the reference count */
	int fd;                   /* the file descriptor */
	unsigned short port;
	bool connected;           /* is the socket connected? */
	bool rx_rdy;              /* ready to read? */
	bool tx_rdy;              /* ready to write? */
	int rx_nrents;            /* number of RX SGs */
	int tx_nrents;            /* number of TX SGs */
	struct sg_ent rx_ents[MAX_SGS];
	struct sg_ent tx_ents[MAX_SGS];

	/* Low-level recv */
	int rx(void);
	/* Low-level send */
	int tx(void);

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

	/* Open a new remote connection */
	int connect(const char *addr, unsigned short port);

	/* Read and write (vector IO support) */
	int read(struct sg_ent *ent);
	int write(struct sg_ent *ent);

	void handler(uint32_t events);

public:
	void get(void); /* Take a new reference */
	void put(void); /* Release a reference */
};

#endif /* SOCKET_HH */
