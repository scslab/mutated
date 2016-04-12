#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "debug.hh"
#include "protocol.hh"
#include "workload.hh"
#include "time.hh"

static void do_work(struct req_pkt *req)
{
    int i;
    workload *w;
    struct timespec ts;

    w = workload_alloc();
    if (!w)
        exit(1);

    if (req->nr > REQ_MAX_DELAYS)
        exit(1);

    for (i = 0; i < req->nr; i++) {
        if (req->delays[i] & REQ_DELAY_SLEEP) {
            us_to_timespec(req->delays[i], &ts);
            nanosleep(&ts, nullptr);
        } else {
            workload_run(w, (req->delays[i] & REQ_DELAY_MASK));
        }
    }

    free(w);
}

static void *thread_handler(void *arg)
{
    int ret, fd = (long)arg;
    struct req_pkt req;
    struct resp_pkt resp;

    while (true) {
      ret = read(fd, (void *)&req, sizeof(req));
      if (ret != sizeof(req)) {
          close(fd);
          return nullptr;
      }

      do_work(&req);

      resp.tag = req.tag;
      ret = write(fd, (void *)&resp, sizeof(resp));
      if (ret != sizeof(resp)) {
          close(fd);
          return nullptr;
      }
    }
}

int main(void)
{
    struct sockaddr_in s_in;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    int server_fd, fd, ret, flag = 1;
    pthread_t tid;

    workload_setup(1000);

    server_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket()");
        exit(1);
    }

    ret = setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&flag,
                     sizeof(int));
    if (ret) {
        perror("unable to set reusable address\n");
    }

    memset(&s_in, 0, sizeof(s_in));
    s_in.sin_family = PF_INET;
    s_in.sin_addr.s_addr = INADDR_ANY;
    s_in.sin_port = htons(8080);

    ret = bind(server_fd, (struct sockaddr *)&s_in, sizeof(s_in));
    if (ret == -1) {
        perror("bind()");
        exit(1);
    }

    ret = listen(server_fd, SOMAXCONN);
    if (ret) {
        perror("listen()");
        exit(1);
    }

    while (1) {
        struct sockaddr_in addr;
        fd = accept(server_fd, (struct sockaddr *)&addr, &addr_len);
        if (fd < 0) {
            perror("accept()");
            exit(1);
        }

        ret =
          setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
        if (ret == -1) {
            perror("setsockopt()");
            exit(1);
        }

        ret = pthread_create(&tid, nullptr, thread_handler, (void *)(long)fd);
        if (ret == -1) {
            perror("pthread_create()");
            exit(1);
        }

        pthread_detach(tid);
    }
}
