#include <cstdio>
#include <cstdlib>

#include "time.hh"

/**
 * timespec_subtract - subtracts timespec y from timespec x
 * @x, @y: the timespecs to subtract
 * @result: a pointer to store the answer
 *
 * WARNING: It's not safe for @result to be @x or @y.
 *
 * Returns 1 if the difference is negative, otherwise 0.
 */
int timespec_subtract(struct timespec *x, struct timespec *y,
                      struct timespec *result)
{
    if (x->tv_nsec < y->tv_nsec) {
        int secs = (y->tv_nsec - x->tv_nsec) / _TIME_NSEC + 1;
        y->tv_nsec -= _TIME_NSEC * secs;
        y->tv_sec += secs;
    }

    if (x->tv_nsec - y->tv_nsec > _TIME_NSEC) {
        int secs = (x->tv_nsec - y->tv_nsec) / _TIME_NSEC;
        y->tv_nsec += _TIME_NSEC * secs;
        y->tv_sec -= secs;
    }

    result->tv_sec = x->tv_sec - y->tv_sec;
    result->tv_nsec = x->tv_nsec - y->tv_nsec;

    return x->tv_sec < y->tv_sec;
}

/**
 * busy_spin - spins the CPU for a given delay
 * @delay: the amount of time to spin (timespec)
 */
void busy_spin(struct timespec *delay)
{
    int ret;
    struct timespec start, now, tmp1, tmp2;

    ret = clock_gettime(CLOCK_MONOTONIC, &start);
    if (ret == -1) {
        perror("clock_gettime()");
        exit(1);
    }

    do {
        ret = clock_gettime(CLOCK_MONOTONIC, &now);
        if (ret == -1) {
            perror("clock_gettime()");
            exit(1);
        }

        if (timespec_subtract(&now, &start, &tmp1)) {
            fprintf(stderr, "clock not monotonic??\n");
            exit(1);
        }
    } while (!(timespec_subtract(delay, &tmp1, &tmp2)));
}
