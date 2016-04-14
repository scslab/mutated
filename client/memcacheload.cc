#include <cstring>
#include <iostream>
#include <functional>
#include <memory>
#include <string>

#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "memcache.hh"
#include "memcacheload.hh"
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
    cerr << "  -k INT: the number of keys to load (default: 100K)" << endl;
    cerr << "  -v INT: the size of the values stored with keys (default: 64KB)" << endl;
    cerr << "  -n INT: the key sequence number to start from (default: 1)" << endl;
    cerr << "  -b INT: the load batch size to use (default: 100)" << endl;
    cerr << "  -e INT: ask server to notify every INT sets of success (default: 25)" << endl;

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
  , keys{100000}
  , valn{64 * 1024}
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
int main(int argc, char* argv[])
{
    Config cfg(argc, argv);
    MemcacheLoad mem(cfg.addr, cfg.port, cfg.keys, cfg.valn, cfg.start, cfg.batch, cfg.notify);
    mem.run();
    return EXIT_SUCCESS;
}

/**
 * Construct a new memcache data loader.
 */
MemcacheLoad::MemcacheLoad(const char* addr, unsigned short port, uint64_t toload, uint64_t valsize, uint64_t startid, uint64_t batch, uint64_t notify)
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
void MemcacheLoad::epoll_watch(int fd, void* data, uint32_t events)
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
            send_request(seqid_++, not loud); // we pre-increment to start keys at 1
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

const char* MemcacheLoad::next_key(uint64_t seqid)
{
    static char key[KEYSIZE];
    snprintf(key, KEYSIZE, "key-%025ld", seqid);
    return key;
}

const char* MemcacheLoad::next_val(uint64_t seqid)
{
    UNUSED(seqid);
    return val_.get();
}

void MemcacheLoad::send_request(uint64_t seqid, bool quiet)
{
    // create our request
    MemcHeader    header;
    MemcExtrasSet extras;
    const char*   key;
    const char*   val;
    MemcPacket    packet;

    header.type       = MEMC_REQUEST;
    header.cmd        = quiet ? MEMC_CMD_SETQ : MEMC_CMD_SET;
    header.keylen     = htons(KEYSIZE);
    header.extralen   = sizeof(extras);
    header.datatype   = 0;
    header.status     = htons(MEMC_OK);
    header.bodylen    = htonl(KEYSIZE + sizeof(extras) + valsize_);
    header.opaque     = 0;
    header.version    = 0;

    extras.flags      = 0;
    extras.expiration = 0;

    key = next_key(seqid);
    val = next_val(seqid);

    packet.header = &header;
    packet.extras = reinterpret_cast<char *>(&extras);
    packet.key    = key;
    packet.value  = val;

    // add header to wire
    size_t n = MEMC_HEADER_SIZE, n1 = n;
    auto wptrs = sock_->write_prepare(n1);
    memcpy(wptrs.first, packet.header, n1);
    if (n != n1) {
        memcpy(wptrs.second, packet.header + n1, n - n1);
    }
    sock_->write_commit(n);

    // add extras to wire
    n = sizeof(extras), n1 = n;
    wptrs = sock_->write_prepare(n1);
    memcpy(wptrs.first, packet.extras, n1);
    if (n != n1) {
        memcpy(wptrs.second, packet.extras + n1, n - n1);
    }
    sock_->write_commit(n);

    // add key to wire
    n = KEYSIZE, n1 = n;
    wptrs = sock_->write_prepare(n1);
    memcpy(wptrs.first, packet.key, n1);
    if (n != n1) {
        memcpy(wptrs.second, packet.key + n1, n - n1);
    }
    sock_->write_commit(n);

    // add value to wire
    n = valsize_, n1 = n;
    wptrs = sock_->write_prepare(n1);
    memcpy(wptrs.first, packet.value, n1);
    if (n != n1) {
        memcpy(wptrs.second, packet.value + n1, n - n1);
    }
    sock_->write_commit(n);

    // add response to read queue
    if (not quiet) {
        ioop io(MEMC_HEADER_SIZE, nullptr, cb_);
        sock_->read(io);
    }
}

void MemcacheLoad::recv_response(Sock* s, void* data, char* seg1, size_t n,
                                 char* seg2, size_t m, int status)
{
    UNUSED(data);
    UNUSED(seg1);
    UNUSED(seg2);

    // sanity checks
    if (sock_.get() != s) { // ensure right callback
        throw runtime_error("MemcacheLoad::recv_response: wrong socket in callback");
    } else if (status != 0) { // just return on error
        return;
    } else if (n + m != MEMC_HEADER_SIZE) { // ensure valid packet
        throw runtime_error("MemcacheLoad::recv_response: unexpected packet size");
    }

    // mark done
    recv_ += notify_;
    onwire_ -= notify_;
}
