// OS functions for current process' environment.

#include <unistd.h> // gethostname, getcwd
#include <stdlib.h> // getenv

// pub bool get_hostname(char *buf, size_t bufsize) {
//     return OS.gethostname(buf, bufsize) == 0;
// }

// Puts the current working directory's path into the buffer.
// Returns false if the buffer is too small.
pub bool getcwd(char *buf, size_t n) {
    return OS.getcwd(buf, n) != NULL;
}

pub const char *getenv(const char *name) {
	return OS.getenv(name);
}
