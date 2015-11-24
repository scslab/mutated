#ifndef CLIENT_HH
#define CLIENT_HH

#include <cstdint>
#include <random>

extern std::mt19937 randgen;

void record_sample(uint64_t service_us, uint64_t wait_us, bool should_measure);

int epoll_watch(int fd, void *data, uint32_t events);

int old_main(int argc, char *argv[]);

#endif /* CLIENT_HH */
