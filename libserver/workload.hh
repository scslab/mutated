#ifndef WORKLOAD_HH
#define WORKLOAD_HH

/**
 * workload.hh - fake CPU work generation
 */

#include <cstdint>

/**
 * A 'workload' is just a block of memory we'll touch to simulate a working
 * sets cache footprint.
 */
using workload = char;

void workload_setup(int lines);
workload *workload_alloc(void);
void workload_run(workload *w, uint64_t us);

#endif /* WORKLOAD_HH */
