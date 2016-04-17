#ifndef LIBSERVER_DEBUG_HH
#define LIBSERVER_DEBUG_HH

/**
 * common.h - general utitilies
 */

#include <stdarg.h>

[[noreturn]] void panic(const char *, ...);

#ifdef __GNUC__
#define UNUSED __attribute__((unused))
#define NOINLINE __attribute__((noinline))
#else
#define UNUSED
#define NOINLINE
#endif

#endif /* LIBSERVER_DEBUG_HH */
