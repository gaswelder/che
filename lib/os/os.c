#include <unistd.h> // gethostname, getcwd
#include <stdlib.h> // getenv

pub bool get_hostname(char *buf, size_t bufsize) {
    return OS.gethostname(buf, bufsize) == 0;
}

pub bool getcwd(char *buf, size_t n) {
    return OS.getcwd(buf, n) != NULL;
}

pub const char *getenv(const char *name) {
	return OS.getenv(name);
}
