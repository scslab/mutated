#ifndef PROTOCOL_HH
#define PROTOCOL_HH

#define REQ_DELAY_SLEEP (1UL << 63)
#define REQ_DELAY_MASK (~(REQ_DELAY_SLEEP))
#define REQ_MAX_DELAYS 16

#include <cstdint>

struct req_pkt
{
    int nr; /* the number of delays */
    int pad;
    uint64_t tag;                    /* a unique indentifier for the request */
    uint64_t delays[REQ_MAX_DELAYS]; /* an array of delays */
} __attribute__((packed));

struct resp_pkt
{
    uint64_t tag;
} __attribute__((packed));

#endif /* PROTOCOL_HH */
