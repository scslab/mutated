#include <cmath>
#include <iostream>

#include <unistd.h>
#include <string.h>

#include "opts.hh"

using namespace std;

/* Microseconds in a second. */
// TODO: Duplicated in client.cc
static constexpr double USEC = 1000000;

/* Fixed arguments required */
static constexpr size_t FIXED_ARGS = 3;

/**
 * Print usage message and exit with status.
 */
static void __printUsage(string prog, int status = EXIT_FAILURE)
{
    if (status != EXIT_SUCCESS) {
        cerr << "invalid arguments!" << endl << endl;
    }

    cerr << "usage: " << prog
         << " [-h] [-r] [-n integer] [-w integer] [-s integer] [-c integer] "
            "[-m string] [-l string] "
            "<ip:port> <req_per_sec> <generator> [<args>]" << endl;
    cerr << "  -h: help" << endl;
    cerr << "  -r: raw machine-readable format" << endl;
    cerr << "  -w: warm-up seconds" << endl;
    cerr << "  -s: measurement sample count" << endl;
    cerr << "  -c: cool-down seconds" << endl;
    cerr << "  -l: label for machine-readable output (-r)" << endl;
    cerr << "  -m: the connection mode ('per_request' or 'round_robin')"
         << endl;
    cerr << "  -n: the number of connections to open (if round robin mode)"
         << endl;

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
  , samples{1000}
  , machine_readable{false}
  , conn_mode{ROUND_ROBIN}
  , conn_cnt{10}
  , gen_argc{0}
  , gen_argv{NULL}
{
    int ret, c;
    opterr = 0;

    while ((c = getopt(argc, argv, "hrw:s:c:l:m:n:")) != -1) {
        switch (c) {
        case 'h':
            __printUsage(argv[0], EXIT_SUCCESS);
        case 'r':
            machine_readable = true;
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

    ret = sscanf(argv[optind + 1], "%lf", &service_us);
    if (ret != 1) {
        __printUsage(argv[0]);
    }

    // TODO: plug into an array of modular generators
    if (strcmp(argv[optind + 2], "synthetic") != 0)
        __printUsage(argv[0]);

    req_s = USEC / service_us;

    gen_argc = argc - optind - FIXED_ARGS;
    gen_argv = &argv[gen_argc];
}
