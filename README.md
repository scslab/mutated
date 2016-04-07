# Mutated

Mutated a high-performance and very accurate load-generator for stressing
servers and measuring their latency behaviour under load.

It tries to avoid mistakes made in previous load-generators such as
[mutilate](https://github.com/leverich/mutilate), to which mutated owes much of
its inspiration and learning to.

## Licensing

This library is BSD-licensed.

## Building

This project uses automake and autoconf for its build system. To build:

``` sh
git submodule init
git submodule update
./libeventm/autogen.sh
./autogen.sh
./configure
make
```

It requires a recent C++11 compiler (GCC or Clang should both work) and has
only been tested on Linux. Support for BSD's should be possible, but will
require abstracting the epoll implementation. Nothing beyond Linux POSIX is
needed.

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

