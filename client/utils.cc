#include <string>
#include <system_error>

#include "utils.hh"

using namespace std;

unsigned int SystemCall(int status, const char * fail, int code)
{
	if (status < 0) {
		code = code == 0 ? errno : code;
		throw system_error(code, system_category(), fail);
	} else {
		return status;
	}
}

unsigned int SystemCall(int status, string fail, int code)
{
	return SystemCall(status, fail.c_str(), code);
}
