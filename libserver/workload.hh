#ifndef WORKLOAD_HH
#define WORKLOAD_HH

/**
 * workload.hh - fake CPU work generation
 */

#include <cstdint>

struct workload {
	// ezyang: This name is misleading, it implies "cache line" but
	// a typical cache line is 4-64 bytes (it's 64 bytes on the
	// Mavericks).
	char lines[];
};

extern struct workload *workload_alloc(void);
extern void workload_run(struct workload *w, uint64_t us);
extern void workload_setup(int lines);
 
#endif /* WORKLOAD_HH */
