#include <cstdio>
#include <cstdlib>

#include "debug.hh"

void NORETURN panic(const char *s, ...)
{
    va_list ap;
    va_start(ap, s);
    fflush(stdout);
    vfprintf(stderr, s, ap);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(ap);

    // Drop us into GDB, if applicable
    abort();
}
