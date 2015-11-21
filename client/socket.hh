#ifndef SOCKET_HH
#define SOCKET_HH

/**
 * socket.h - async socket I/O support
 */

#include <cstdint>
#include <cstring>

struct sock;

struct sg_ent {
	char   *buf;
	size_t len;
	void   *data;
	void   (*complete) (struct sock *s, void *data, int ret);
};

#define MAX_SGS 64

class sock {
	int ref_cnt;              /* the reference count */
	int fd;                   /* the file descriptor */
	unsigned short port;
	unsigned int connected:1; /* is the socket connected? */
	unsigned int rx_rdy:1;    /* ready to read? */
	unsigned int tx_rdy:1;    /* ready to write? */
	int rx_nrents;            /* number of RX SGs */
	int tx_nrents;            /* number of TX SGs */
	struct sg_ent rx_ents[MAX_SGS];
	struct sg_ent tx_ents[MAX_SGS];
};

struct sock *socket_alloc(void);
int socket_read(struct sock *s, struct sg_ent *ent);
int socket_write(struct sock *s, struct sg_ent *ent);
int socket_create(struct sock *s, const char *addr, unsigned short port);
void socket_get(struct sock *s);
void socket_put(struct sock *s);

void socket_handler(struct sock *s, uint32_t events);

#endif /* SOCKET_HH */
