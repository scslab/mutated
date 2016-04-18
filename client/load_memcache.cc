#include <cstring>
#include <iostream>
#include <functional>
#include <memory>
#include <string>

#include <arpa/inet.h>
#include <inttypes.h>
#include <unistd.h>

#include "linux_compat.hh"
#include "load_memcache.hh"
#include "memcache.hh"
#include "util.hh"

// TODO:
// - avoid memcpy to construct packet
// - support distributions for key/value size

using namespace std;
using namespace std::placeholders;

/* Fixed arguments required. */
static constexpr size_t FIXED_ARGS = 1;

/**
 * Print usage message and exit with status.
 */
static void __printUsage(string prog, int status = EXIT_FAILURE)
{
    if (status != EXIT_SUCCESS) {
        cerr << "invalid arguments!" << endl
             << endl;
    }

    cerr << "Usage: " << prog << " [options] <ip:port>" << endl;
    cerr << endl;
    cerr << "Options:" << endl;
    cerr << "  -h    : help" << endl;
    cerr << "  -k INT: number of keys to load (default: 10K)" << endl;
    cerr << "  -v INT: size of the values (default: 4KB)" << endl;
    cerr << "  -n INT: starting key sequence number (default: 1)" << endl;
    cerr << "  -b INT: load batch size to use (default: 100)" << endl;
    cerr << "  -e INT: ask server to notify every INT sets of success "
            "(default: 25)" << endl;

    exit(status);
}

/**
 * MemcacheLoad options.
 */
class Config
{
  public:
    char addr[256];  /* the server address */
    uint16_t port;   /* the server port */
    uint64_t keys;   /* number of keys to load */
    uint64_t valn;   /* length of values */
    uint64_t start;  /* starting sequence number */
    uint64_t batch;  /* load batch size */
    uint64_t notify; /* notify window */

    Config(int argc, char *argv[]);
};

/**
 * Parse command line.
 */
Config::Config(int argc, char *argv[])
  : port{0}
  , keys{10000}
  , valn{4 * 1024}
  , start{1}
  , batch{100}
  , notify{25}
{
    int ret, c;
    opterr = 0;

    while ((c = getopt(argc, argv, "hk:v:n:b:e:")) != -1) {
        switch (c) {
        case 'h':
            __printUsage(argv[0], EXIT_SUCCESS);
        case 'k':
            keys = atoi(optarg);
            break;
        case 'v':
            valn = atoi(optarg);
            break;
        case 'n':
            start = atoi(optarg);
            break;
        case 'b':
            batch = atoi(optarg);
            break;
        case 'e':
            notify = atoi(optarg);
            break;
        default:
            __printUsage(argv[0]);
        }
    }

    if ((unsigned int)(argc - optind) < FIXED_ARGS) {
        __printUsage(argv[0]);
    }

    // NOTE: keep 255 in sync with addr buffer size.
    ret = sscanf(argv[optind + 0], "%255[^:]:%20hu", addr, &port);
    if (ret != 2) {
        __printUsage(argv[0]);
    }
}

/**
 * Main method -- launch memcacheload.
 */
int main(int argc, char *argv[])
{
    Config cfg(argc, argv);
    MemcacheLoad mem(cfg.addr, cfg.port, cfg.keys, cfg.valn, cfg.start,
                     cfg.batch, cfg.notify);
    mem.run();
    return EXIT_SUCCESS;
}

/**
 * Construct a new memcache data loader.
 */
MemcacheLoad::MemcacheLoad(const char *addr, unsigned short port,
                           uint64_t toload, uint64_t valsize, uint64_t startid,
                           uint64_t batch, uint64_t notify)
  : epollfd_{SystemCall(epoll_create1(0), "MemcacheLoad: epoll_create1()")}
  , sock_{make_unique<Sock>()}
  , cb_{bind(&MemcacheLoad::recv_response, this, _1, _2, _3, _4, _5, _6, _7)}
  , toload_{toload}
  , sent_{0}
  , recv_{0}
  , valsize_{valsize}
  , val_{make_unique<char[]>(valsize_)}
  , seqid_{startid}
  , batch_{batch}
  , onwire_{0}
  , notify_{notify}
{
    sock_->connect(addr, port);
    epoll_watch(sock_->fd(), nullptr, EPOLLIN | EPOLLOUT);
    memset(val_.get(), 'a', valsize_);
}

/**
 * epoll_watch - registers a file descriptor for epoll events.
 * @fd: the file descriptor.
 * @data: a cookie for the event.
 * @event: the event mask.
 */
void MemcacheLoad::epoll_watch(int fd, void *data, uint32_t events)
{
    epoll_event ev;
    ev.events = events | EPOLLET;
    ev.data.ptr = data;
    SystemCall(epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &ev),
               "MemcacheLoad::epoll_watch: epoll_ctl()");
}

/**
 * Run the generator, loading data.
 */
void MemcacheLoad::run(void)
{
    constexpr size_t MAX_EVENTS = 4096;
    epoll_event events[MAX_EVENTS];
    int nfds;

    while (true) {
        while (sent_ < toload_ and onwire_ < batch_) {
            bool loud = ((sent_ + 1) % notify_) == 0 or sent_ == (toload_ - 1);
            send_request(seqid_++,
                         not loud); // we pre-increment to start keys at 1
            sent_++;
            onwire_++;
        }

        if (recv_ >= toload_) {
            break;
        }

        nfds = SystemCall(epoll_wait(epollfd_, events, MAX_EVENTS, -1),
                          "MemcacheLoad::run: epoll_wait()");
        for (int i = 0; i < nfds; i++) {
            epoll_event &ev = events[i];
            sock_->run_io(ev.events);
        }
    }
}

const char *MemcacheLoad::next_key(uint64_t seqid)
{
    static char key[KEYSIZE + 1]; // +1 for '\0'
    snprintf(key, KEYSIZE + 1, "key-%026" PRIu64, seqid);
    return key;
}

const char *MemcacheLoad::next_val(uint64_t seqid)
{
    UNUSED(seqid);
    return val_.get();
}

void MemcacheLoad::send_request(uint64_t seqid, bool quiet)
{
    // create our request
    MemcExtrasSet extras{};
    MemcHeader header = MemcRequest(quiet ? MemcCmd::Setq : MemcCmd::Set,
                                    sizeof(extras), KEYSIZE, valsize_);
    const char *key = next_key(seqid);
    const char *val = next_val(seqid);
    header.hton();

    // add request to wire
    sock_->write(&header, MemcHeader::SIZE);
    sock_->write(&extras, sizeof(extras));
    sock_->write(key, KEYSIZE);
    sock_->write(val, valsize_);

    // add response to read queue
    if (not quiet) {
        ioop io(MemcHeader::SIZE, cb_, 0, nullptr, nullptr);
        sock_->read(io);
    }
}

size_t MemcacheLoad::recv_response(Sock *s, void *data, char *seg1, size_t n,
                                   char *seg2, size_t m, int status)
{
    UNUSED(data);
    UNUSED(seg1);
    UNUSED(seg2);

    // sanity checks
    if (sock_.get() != s) { // ensure right callback
        throw runtime_error(
          "MemcacheLoad::recv_response: wrong socket in callback");
    } else if (status != 0) { // just return on error
        return 0;
    } else if (n + m != MemcHeader::SIZE) { // ensure valid packet
        throw runtime_error(
          "MemcacheLoad::recv_response: unexpected packet size");
    }

    // mark done
    recv_ += notify_;
    onwire_ -= notify_;
    return 0;
}
