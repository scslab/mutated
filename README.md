# Mutated

Mutated a high-performance and very accurate load-generator for stressing
servers and measuring their latency behaviour under load.

It tries to avoid mistakes made in previous load-generators such as
[mutilate](https://github.com/leverich/mutilate), to which mutated owes much of
its inspiration and learning to.

## Licensing

Mutated is BSD-licensed.

## Using

``` sh
Usage: ./client/client [options] <ip:port> <generator> <exp. service us> <req/sec>

Options:
  -h    : help
  -r    : print raw samples
  -e    : use Shinjuku's epoll_spin() system call
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
computation time). This argument is only supported by the synthetic protocol.
The synthetic protocol includes this value in a packet and the server does that
wall-clock amount of work (on a single core) before replying.

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

## Design

We take several design lessons from our previous experience building and
working with [mutilate](https://github.com/leverich/mutilate):

* **Single threaded**: launch multiple mutated processes to generate more load
  if required.
* **Multi-agent orchestration out-of-band**: mutilate supported a network
  protocol to coordinate a set of mutilate processes to generate load, allowing
  the use of multiple machines. Mutated can be used this way too but pushes
  this out of the core code, instead just relying on generic process
  coordination using shell scripts.
* **Data loading out-of-band**: mutilate was fairly specific to memcached,
  supporting loading an initial data set into the server. Mutated avoids this,
  instead relying on separate tools (although distributed with mutated) for
  loading the testing data before starting mutated.
* **Collect all results**: we record every single sample. We want to be able to
  find any and all latency issues.
* **Open system**: we generate packets as an open system. In particular, we do
  this by fixing the entire packet schedule ahead of time so that we know when
  we aren't hitting our expected transmission rate, and to keep the hot-path
  (packet transmission) as fast as possible.
* **Fast hot-path**: we try to avoid any allocation and as much work as
  possible on the packet transmission path (hot-path) to ensure as little noise
  caused by mutated itself.

The overall theme compared to mutilate is one of simplifying and reducing the
core concern to only packet generation and latency sampling. Agent
coordination, initial test-data loading, post-sample result processing are all
pushed into separate tools to keep mutated as lean and simple as possible.

## Building

This project uses automake and autoconf for its build system. To build:

``` sh
git submodule update --init
./autogen.sh
./configure
make
```

It requires a recent C++11 compiler (GCC or Clang should both work) and has
only been tested on Linux. Support for BSD's should be possible, but will
require abstracting the epoll implementation.

## Coding style

We use `clang-format` to enforce a coding style and avoid bike-shedding
arguments. Please run `make format` to format the code before committing your
changes.

We also make use of `cppcheck` and `clang-check` static analysis tools to look
for bugs. Please check your changes with `make cppcheck` and `make clang-scan`.
We keep the code free of all warnings at all times.

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

