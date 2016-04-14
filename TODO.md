# TODO

Todo notes for mutated.

## Core

* Cleanup how multiple protocols are supported.
* Pull common code out of client into own lib.
* Should loaders be explicitly supported or out-of-band?
* Argument parsing for different protocols.

## Memcached

* Loader should support key-value size distributions beyond fixed.
* Pull loader into own folder (core cleaning needed).
* Generator should perform GETs on the key-scheme loaded by the loader.
* Generator should support setting hit/miss ratio.
* Generator should support a mixed GET/SET workload.
* Generator should support distributions beyond fixed for hit/miss ratio and
  GET/SET workload.
* Generator SET workload should support distributions beyond fixed for choosing
  value sizes.

