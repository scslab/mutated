#ifndef MUTATED_MEMCACHELOAD_HH
#define MUTATED_MEMCACHELOAD_HH

#include <cstdint>
#include <memory>

#include "socket_buf.hh"

class MemcacheLoad
{
  private:
    static constexpr std::size_t KEYSIZE = 30;

    unsigned int epollfd_;
    std::unique_ptr<Sock> sock_;
    ioop::ioop_cb cb_;
    uint64_t toload_;
    uint64_t sent_;
    uint64_t recv_;
    uint64_t valsize_;
    std::unique_ptr<char[]> val_;
    uint64_t seqid_;
    uint64_t batch_;
    uint64_t onwire_;
    uint64_t notify_;

    void epoll_watch(int fd, void *data, uint32_t events);
    void send_request(uint64_t seqid, bool quiet);
    void recv_response(Sock *s, void *data, char *seg1, size_t n, char *seg2, size_t m, int status);

    const char* next_key(uint64_t seqid);
    const char* next_val(uint64_t seqid);

  public:
    MemcacheLoad(const char* addr, unsigned short port, uint64_t toload, uint64_t valsize, uint64_t startid, uint64_t batch, uint64_t notify);
    void run(void);
};

#endif /* MUTATED_MEMCACHELOAD_HH */
