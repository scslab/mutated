#ifndef COMMON_HH
#define COMMON_HH

/**
 * common.h - general utitilies
 */

#include <stdarg.h>

void panic(const char*, ...);

#ifdef __GNUC__
#  define UNUSED   __attribute__((unused))
#  define NOINLINE __attribute__((noinline))
#else
#  define UNUSED
#  define NOINLINE
#endif

#endif /* COMMON_HH */
