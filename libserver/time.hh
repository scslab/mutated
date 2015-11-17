#ifndef TIME_HH
#define TIME_HH

/**
 * time.h - library routines for managing time
 */

#include <ctime>
#include <cstdint>

#define USEC	1000000
#define NSEC	1000000000

/**
 * us_to_timespec - converts microseconds to a timespec
 * @us: number of microseconds
 * @t: the storage timespec
 */
inline void us_to_timespec(uint64_t us, struct timespec *t)
{
	t->tv_sec = us / USEC;
	t->tv_nsec = (us - t->tv_sec * USEC) * (NSEC / USEC);
}

/**
 * timespec_to_us - converts a timespec to microseconds
 * @t: the timespec
 *
 * Returns microseconds.
 */
inline uint64_t timespec_to_us(struct timespec *t)
{
	return t->tv_sec * USEC + t->tv_nsec / (NSEC / USEC);
}

/**
 * timespec_to_ns - converts a timespec to nanoseconds
 * @t: the timespec
 *
 * Returns nanoseconds.
 */
inline uint64_t timespec_to_ns(struct timespec *t)
{
	return t->tv_sec * NSEC + t->tv_nsec;
}

int timespec_subtract(struct timespec *x, struct timespec *y,
                      struct timespec *result);
void busy_spin(struct timespec *delay);

#endif /* TIME_HH */
