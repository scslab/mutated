#include <cmath>
#include <iostream>

#include <unistd.h>
#include <string.h>

#include "opts.hh"

using namespace std;

/* Fixed arguments required. */
static constexpr size_t FIXED_ARGS = 4;

/* Default number of seconds to sample for. */
static constexpr uint64_t DEFAULT_SAMPLE_S = 10;

/**
 * Print usage message and exit with status.
 */
static void __printUsage(string prog, int status = EXIT_FAILURE)
{
    if (status != EXIT_SUCCESS) {
        cerr << "invalid arguments!" << endl
             << endl;
    }

    cerr << "Usage: " << prog << " [options] "
         << "<ip:port> <generator> <exp. service us> <req/sec>" << endl
         << endl;
    cerr << "Options:" << endl;
    cerr << "  -h    : help" << endl;
    cerr << "  -r    : print raw samples" << endl;
    cerr << "  -e    : use Shinjuku's epoll_spin() system call" << endl;
    cerr << "  -w INT: warm-up seconds (default: 5s)" << endl;
    cerr << "  -c INT: cool-down seconds (default: 5s)" << endl;
    cerr << "  -s INT: measurement sample count (default: 10s worth)" << endl;
    cerr << "  -l STR: label for machine-readable output (-r)" << endl;
    cerr << "  -m OPT: connection mode (default: round_robin)" << endl;
    cerr << "  -d OPT: the service time distribution (default: exponential)"
         << endl;
    cerr << "  -n INT: the number of connections to open (round robin/random "
            "mode)" << endl;
    cerr << endl;
    cerr << "  generators: synthetic, memcache" << endl;
    cerr << "  connection modes: per_request, round_robin, random" << endl;
    cerr << "  service distribution: fixed, exp, lognorm" << endl;

    exit(status);
}

/**
 * Parse command line.
 */
Config::Config(int argc, char *argv[])
  : port{0}
  , label{"default"}
  , service_us{0}
  , req_s{0}
  , warmup_seconds{5}
  , cooldown_seconds{5}
  , samples{0}
  , machine_readable{false}
  , use_epoll_spin{false}
  , conn_mode{ROUND_ROBIN}
  , conn_cnt{10}
  , service_dist{EXPONENTIAL}
  , protocol{SYNTHETIC}
  , gen_argc{0}
  , gen_argv{NULL}
{
    int ret, c;
    opterr = 0;

    while ((c = getopt(argc, argv, "hrew:s:c:l:m:d:n:")) != -1) {
        switch (c) {
        case 'h':
            __printUsage(argv[0], EXIT_SUCCESS);
        case 'r':
            machine_readable = true;
            break;
        case 'e':
            use_epoll_spin = true;
            break;
        case 'w':
            warmup_seconds = atoi(optarg);
            break;
        case 's':
            samples = atoi(optarg);
            break;
        case 'c':
            cooldown_seconds = atoi(optarg);
            break;
        case 'l':
            label = optarg;
            break;
        case 'm':
            if (!strcmp(optarg, "per_request"))
                conn_mode = PER_REQUEST;
            else if (!strcmp(optarg, "round_robin"))
                conn_mode = ROUND_ROBIN;
            else if (!strcmp(optarg, "random"))
                conn_mode = RANDOM;
            else
                __printUsage(argv[0]);
            break;
        case 'd':
            if (!strcmp(optarg, "fixed"))
                service_dist = FIXED;
            else if (!strcmp(optarg, "exp"))
                service_dist = EXPONENTIAL;
            else if (!strcmp(optarg, "lognorm"))
                service_dist = LOG_NORMAL;
            else
                __printUsage(argv[0]);
            break;
        case 'n':
            conn_cnt = atoi(optarg);
            break;
        default:
            __printUsage(argv[0]);
        }
    }

    if ((unsigned int)(argc - optind) < FIXED_ARGS) {
        __printUsage(argv[0]);
    }

    ret = sscanf(argv[optind + 0], "%[^:]:%hu", addr, &port);
    if (ret != 2) {
        __printUsage(argv[0]);
    }

    if (strcmp(argv[optind + 1], "synthetic") == 0) {
        protocol = SYNTHETIC;
    } else if (strcmp(argv[optind + 1], "memcache") == 0) {
        protocol = MEMCACHE;
    } else {
        __printUsage(argv[0]);
    }

    ret = sscanf(argv[optind + 2], "%lf", &service_us);
    if (ret != 1) {
        __printUsage(argv[0]);
    }

    ret = sscanf(argv[optind + 3], "%lf", &req_s);
    if (ret != 1) {
        __printUsage(argv[0]);
    }

    if (samples == 0) {
        samples = req_s * DEFAULT_SAMPLE_S;
    }
    gen_argc = argc - optind - FIXED_ARGS;
    gen_argv = &argv[gen_argc];
}
