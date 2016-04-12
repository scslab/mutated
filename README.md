# Mutated

Mutated a high-performance and very accurate load-generator for stressing
servers and measuring their latency behaviour under load.

It tries to avoid mistakes made in previous load-generators such as
[mutilate](https://github.com/leverich/mutilate), to which mutated owes much of
its inspiration and learning to.

## Licensing

This library is BSD-licensed.

## Using

``` sh
Usage: ./client/client [options] <ip:port> <generator> <exp. service us> <req/sec>

Options:
  -h    : help
  -r STR: raw machine-readable format
  -w INT: warm-up seconds (default: 5s)
  -c INT: cool-down seconds (default: 5s)
  -s INT: measurement sample count (default: 10s worth)
  -l STR: label for machine-readable output (-r)
  -m OPT: connection mode (default: round_robin)
  -d OPT: the service time distribution (default: exponential)
  -n INT: the number of connections to open (round robin/random mode)

  generators: synthetic, memcache
  connection modes: per_request, round_robin, random
  service distribution: fixed, exp, lognorm
```

Where the `exp. service us` argument specifies how long we expect the
application packet to take to process once received by the server (this is,
computation time).

Once all the requested samples have been collected, output as follows will be
produced:

``` sh
#reqs/s    (ideal)  min  avg      std      99th  99.9th  max   min  avg     std     99th  99.9th  max
10.260609  10.0000  183  1690.05  1430.51  6711  8665    8665  0    464.02  311.93  1746  1938    1938
```

The first column gives the achieved requests/sec while, the second gives the
targeted request/sec rate. The first set of columns (`min`, `avg`, `std`,
`99th`, `99.9th` and `max`) gives the complete RTT for the application packets,
while the second set of columns gives just the queueing delay.

The queueing delay can only be calculated for protocols that explicitly support
it (the service time for an application must be known a-priori).

## Building

This project uses automake and autoconf for its build system. To build:

``` sh
git submodule update --init
./libeventm/autogen.sh
./autogen.sh
./configure
make
```

It requires a recent C++11 compiler (GCC or Clang should both work) and has
only been tested on Linux. Support for BSD's should be possible, but will
require abstracting the epoll implementation.

## Get involved!

We are happy to receive bug reports, fixes, documentation enhancements,
and other improvements.

Please report bugs via the
[github issue tracker](http://github.com/scslab/mutated/issues).

Master [git repository](http://github.com/scslab/mutated):

* `git clone git://github.com/scslab/mutated.git`

## Authors

Please see [AUTHORS](AUTHORS) for the list of contributors. In general though,
mutated is written by the [Stanford Secure Computing
Systems](http://www.scs.stanford.edu/) group.

