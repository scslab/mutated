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
#include "socket.hh"
#include "util.hh"

using namespace std;

/* Memcache read handler */
static void __read_completion_handler2(Sock *sock, void *data, int status)
{
    // TODO: IMPLEMENT!
    UNUSED(status);

    char * req = (char *) data;
    cout << "Response: " << req << endl;
    delete req;
    sock->put();
}

/* Constructor */
memcache::memcache(const Config & cfg)
  : cfg_(cfg)
{
    // TODO: IMPLEMENT!
}

void memcache::send_request(Sock *sock, bool should_measure, request_cb cb)
{
    // TODO: IMPLEMENT!
    UNUSED(should_measure);
    UNUSED(cb);

    // create our request
    const char *req = "get a\r\n";

    // add req to write queue
    vio ent((char *)req, strlen(req));
    sock->write(ent);

    // add response to read queue
    ent.buf = new char[100];
    ent.len = 5;
    ent.cb_data = ent.buf;
    ent.complete = &__read_completion_handler2;
    sock->read(ent);
}
