#include <chrono>
#include <functional>
#include <iostream>
#include <stdexcept>

#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>

#include "memcache.hh"
#include "gen_memcache.hh"
#include "socket_buf.hh"
#include "util.hh"

using namespace std;
using namespace std::placeholders;

// XXX: Future work:
// - Support choosing key with a distribution
// - Support variable value size with SET workload (distribution)

static constexpr std::size_t KEYLEN = 30;
using keyarray =
  char (*)[KEYLEN + 1]; // can't type without a typdef it seems...

static bool kv_setup_ = false;
static keyarray keys_ = nullptr;
static char *val_ = nullptr;

/**
 * Create a memcache request for a given key id.
 */
static void create_key(char *buf, uint64_t id)
{
    static_assert(KEYLEN >= 30, "keys are 30 chars long, not enough space");
    snprintf(buf, KEYLEN + 1, "key-%026" PRIu64, id);
}

/**
 * Construct.
 */
memcache::memcache(const Config &cfg, std::mt19937 &&rand) noexcept
  : cfg_{cfg},
    rand_{move(rand)},
    setget_{0, 1.0},
    rcb_{bind(&memcache::recv_response, this, _1, _2, _3, _4, _5, _6, _7)},
    tcb_{bind(&memcache::sent_request, this, _1, _2, _3)},
    requests_{},
    seqid_{rand_()} // start from random sequence id
{
    UNUSED(cfg_);

    // create all needed requests upfront
    if (not kv_setup_) {
        kv_setup_ = true;
        keys_ = new char[cfg.records][KEYLEN + 1];
        for (size_t i = 1; i <= cfg.records; i++) {
            create_key(keys_[i - 1], i);
        }
        val_ = new char[cfg.valsize];
        memset(val_, 'a', cfg.valsize);
    }
}

MemcCmd memcache::choose_cmd(void)
{
    if (setget_(rand_) < cfg_.setget) {
        return MemcCmd::Set;
    } else {
        return MemcCmd::Get;
    }
}

char *memcache::choose_key(uint64_t id, uint16_t &n)
{
    n = KEYLEN;
    return keys_[id % cfg_.records];
}

char *memcache::choose_val(uint64_t id, uint32_t &n)
{
    UNUSED(id);
    n = cfg_.valsize;
    return val_;
}

/**
 * Generate and send a new request.
 */
uint64_t memcache::_send_request(bool measure, request_cb cb)
{
    uint64_t id = seqid_++;
    uint16_t keylen;
    uint32_t bodlen;
    char *key;

    // create our request
    MemcCmd op = choose_cmd();
    key = choose_key(id, keylen);

    // add req to write queue
    if (op == MemcCmd::Get) {
        sock_.write_emplace<MemcHeader>(MemcType::Request, op, 0, keylen, 0);
        sock_.write(key, keylen);
        bodlen = keylen;
    } else {
        sock_.write_emplace<MemcHeader>(
          MemcType::Request, op, sizeof(MemcExtrasSet), keylen, cfg_.valsize);
        sock_.write_emplace<MemcExtrasSet>();
        sock_.write(key, keylen);

        // just write random bytes for the value
        size_t vn = cfg_.valsize;
        sock_.write_prepare(vn);
        sock_.write_commit(cfg_.valsize);
        bodlen = keylen + sizeof(MemcExtrasSet) + cfg_.valsize;
    }

    // setup timestamps
    memreq &req = requests_.queue_emplace(op, measure, cb);
    req.start_ts = generator::clock::now();
    sock_.write_cb_point(tcb_, &req);

    // try transmission
    sock_.try_tx();

    // add response to read queue
    ioop_rx io(MemcHeader::SIZE, rcb_, 0, nullptr, &req);
    sock_.read(io);

    return MemcHeader::SIZE + bodlen;
}

/**
 * Handle marking a generated memcache request as sent.
 */
void memcache::sent_request(Sock *s, void *data, int status)
{
    if (&sock_ != s) { // ensure right callback
        throw runtime_error(
          "memcache::sent_request: wrong socket in callback");
    } else if (status != 0) { // just return on error
        return;
    }

    // add in sent timestamp to packet
    memreq *req = reinterpret_cast<memreq *>(data);
    req->sent_ts = generator::clock::now();
}

/**
 * Handle parsing a response from a previous request.
 */
size_t memcache::recv_response(Sock *s, void *data, char *seg1, size_t n,
                               char *seg2, size_t m, int status)
{
    if (&sock_ != s) { // ensure right callback
        throw runtime_error(
          "memcache::recv_response: wrong socket in callback");
    } else if (status != 0) { // just delete on error
        requests_.drop(1);
        return 0;
    } else if (n + m != MemcHeader::SIZE) { // ensure valid packet
        throw runtime_error("memcache::recv_response: unexpected packet size");
    }

    // calculate measurement
    const memreq &req = requests_.dequeue_one();
    if (data != &req) {
        throw runtime_error(
          "memcache::recv_response: wrong response-request packet match");
    }
    auto now = generator::clock::now();

    // client-side queue time
    auto delta = req.sent_ts - req.start_ts;
    if (delta <= generator::duration(0)) {
        throw std::runtime_error(
          "memcache::recv_response: sent before it was generated");
    }
    uint64_t queue_us =
      chrono::duration_cast<generator::duration>(delta).count();

    // service time
    delta = now - req.start_ts;
    if (delta <= generator::duration(0)) {
        throw std::runtime_error(
          "memcache::recv_response: arrived before it was sent");
    }
    uint64_t service_us =
      chrono::duration_cast<generator::duration>(delta).count();

    // parse packet - need to drop body
    uint32_t bodylen = 0;
    if (req.op != MemcCmd::Set) {
        if (seg2 == nullptr) {
            MemcHeader *hdr = reinterpret_cast<MemcHeader *>(seg1);
            bodylen = ntohl(hdr->bodylen);
        } else {
            MemcHeader hdr;
            memcpy(&hdr, seg1, n);
            memcpy(&hdr + n, seg2, m);
            bodylen = ntohl(hdr.bodylen);
        }
    }

    // record result
    req.cb(this, queue_us, service_us, 0, MemcHeader::SIZE + bodylen,
           req.measure);

    return bodylen;
}
