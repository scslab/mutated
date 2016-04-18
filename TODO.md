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
* Support setting hit/miss ratio - maybe enough to just set different key set?
* Support distributions for schedule, key choice, and value size.

Mutilate Distributions:
* Request schedule  - fixed, uniform, normal, exponential, pareto, gev
* Choose operation  - fixed, uniform
* Choose key        - fixed
* Choose key sie    - fixed, uniform, normal, exponential, pareto, gev
* Choose value size - fixed, uniform, normal, exponential, pareto, gev

Mutated Distributions:
* Request schedule  - fixed, exponential, lognorm
* Choose operation  - fixed, uniform
* Choose key        - fixed
* Choose key size   - fixed
* Choose value size - fixed

