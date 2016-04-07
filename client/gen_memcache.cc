/*
 * generator -- creates read and write requests on the specified socket
 * conforming to the memcache protocol.
 */

#include <chrono>
#include <iostream>
#include <stdexcept>

#include <errno.h>
#include <sys/epoll.h>

#include "gen_memcache.hh"
#include "socket2.hh"
#include "util.hh"

using namespace std;

static constexpr size_t MAX_MEM_RESP = 100;

struct memreq
{
    using request_cb = generator::request_cb;
    using time_point = generator::time_point;

    time_point start_ts;
    bool measure;
    request_cb cb;
    char resp[MAX_MEM_RESP];
};

static const char *getreq = "get a\r\n";

/* Memcache read handler */
static void __read_completion_handler(Sock *sock, void *data, int status)
{
    // if (status != 0) {
    //     sock->put();
    //     delete (memreq *) data;
    //     return;
    // }
    //
    // memreq *req = (memreq *) data;
    //
    // auto now = generator::clock::now();
    // auto delta = now - req->start_ts;
    // if (delta <= generator::duration(0)) {
    //     throw std::runtime_error(
    //       "__read_completion_handler: sample arrived before it was sent");
    // }
    // uint64_t service_us =
    //   chrono::duration_cast<generator::duration>(delta).count();
    //
    // // req->cb(service_us, 0, req->measure);
    // // char *req = (char *)data;
    // // cout << "Response: " << req << endl;
    // // delete req;
    // sock->put();
}

/* Constructor */
memcache::memcache(const Config &cfg)
  : cfg_(cfg)
{
}

void memcache::send_request(Sock *sock, bool should_measure, request_cb cb)
{
    // // create our request
    // memreq *req = new memreq();
    // req->start_ts = generator::clock::now();
    // req->measure = should_measure;
    // req->cb = cb;
    //
    // // add req to write queue
    // vio ent((char *) getreq, strlen(getreq));
    // sock->write(ent);
    //
    // // add response to read queue
    // ent.buf = req->resp;
    // ent.len = 5;
    // ent.cb_data = req;
    // ent.complete = &__read_completion_handler;
    // sock->read(ent);

    // // TODO: IMPLEMENT!
    // UNUSED(should_measure);
    // UNUSED(cb);
    //
    // // create our request
    // const char *req = "get a\r\n";
    //
    // // add req to write queue
    // ioop ent((char *)req, strlen(req));
    // sock->write(ent);
    //
    // // add response to read queue
    // ent.buf = new char[100];
    // ent.len = 5;
    // ent.cb_data = ent.buf;
    // ent.complete = &__read_completion_handler2;
    // sock->read(ent);
}
