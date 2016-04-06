#ifndef MUTATED_UTILS_HH
#define MUTATED_UTILS_HH

#include <string>

#define UNUSED(x) ((void)(x))

unsigned int SystemCall(int status, const char *fail, int code = 0);
unsigned int SystemCall(int status, std::string &fail, int code = 0);

#endif /* MUTATED_UTILS_HH */
