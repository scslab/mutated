# TODO

Todo notes for mutated.

## Core

* Cleanup how multiple protocols are supported.
* Pull common code out of client into own lib.
* Should loaders be explicitly supported or out-of-band?
* Argument parsing for different protocols.
* Support varying (how to specify?) workload intensity (i.e., not a constant
  req/s rate).

## Cross-OS

* Support FreeBSD/OSX.

## Memcached

Loader:
* Support key-value size distributions beyond fixed.
* Pull loader into own folder (core cleaning needed).

Generator:
* Perform GETs on the key-scheme loaded by the loader.
* Support setting hit/miss ratio.
* Support a mixed GET/SET workload.
* Support distributions beyond fixed for hit/miss ratio and GET/SET workload.
* SET workload should support distributions beyond fixed for choosing value
  sizes.

