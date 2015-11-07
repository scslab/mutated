#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <ctime>

#include "common.hh"
#include "time.hh"
#include "workload.hh"

// Number of memory writes to perform when calibrating iter_per_us, see
// workload_calibrate().
#define N	50000000

// Filled in by workload_setup()
static int dlines = -1;    // how large is memory workspace
static double iter_per_us; // how many iterations in a microsecond of work

static inline uint64_t __mm_crc32_u64(uint64_t crc, uint64_t val)
{
	asm("crc32q %1, %0" : "+r" (crc) : "rm" (val));
	return crc;
}

/**
 * Model a memory workload by writing to n random bytes in some
 * malloc'd memory.  We use the crc32q instruction to
 * generate "random" numbers, although the main point of doing this
 * is to make sure we are touching different cache lines, not
 * because we /actually/ need randomness.
 */
static void __attribute__ ((noinline)) __workload_run(struct workload *w, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		w->lines[__mm_crc32_u64(0xDEADBEEF, i) % dlines] = 0x0A;
	}
}

void workload_run(struct workload *w, uint64_t us)
{
	int n = (int) (iter_per_us * (double) us);

	__workload_run(w, n);
}

static void workload_calibrate(struct workload *w)
{
	int ret;
	struct timespec start, finish, delta;
	uint64_t delta_ns;

	ret = clock_gettime(CLOCK_MONOTONIC, &start);
	if (ret == -1) {
		perror("clock_gettime()");
		exit(1);
	}

	__workload_run(w, N);

	ret = clock_gettime(CLOCK_MONOTONIC, &finish);
	if (ret == -1) {
		perror("clock_gettime()");
		exit(1);
	}

	if (timespec_subtract(&finish, &start, &delta)) {
		fprintf(stderr, "clock not monotonic???\n");
		// ezyang: Exit here?
	}

	delta_ns = timespec_to_ns(&delta);

	iter_per_us = ((double) NSEC / (double) USEC) *
		      ((double) N / (double) delta_ns);

	fprintf(stderr, "calibration: %f iterations / microsecond\n", iter_per_us);
}

struct workload * workload_alloc(void)
{
	assert(dlines > 0);
	return (struct workload *) malloc(sizeof(char) * dlines);
}

void workload_setup(int lines)
{
	struct workload *w;
	dlines = lines * 64;

	w = workload_alloc();
	if (!w)
		panic("workload_setup: not enough memory");

	workload_calibrate(w);
	free(w);
}
