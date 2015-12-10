#ifndef UTILS_HH
#define UTILS_HH

#include <string>

unsigned int SystemCall(int status, const char * fail, int code = 0);
unsigned int SystemCall(int status, std::string & fail, int code = 0);

#endif /* UTILS_HH */
