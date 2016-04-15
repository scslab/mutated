#ifndef LIBSERVER_DEBUG_HH
#define LIBSERVER_DEBUG_HH

/**
 * common.h - general utitilies
 */

#include <stdarg.h>

void panic(const char *, ...);

#ifdef __GNUC__
#define UNUSED __attribute__((unused))
#define NOINLINE __attribute__((noinline))
#define NORETURN __attribute__((noreturn))
#else
#define UNUSED
#define NOINLINE
#define NORETURN
#endif

#endif /* LIBSERVER_DEBUG_HH */
