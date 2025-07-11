// OS functions for current process' environment.

#include <unistd.h> // gethostname, getcwd
#include <stdlib.h> // getenv

pub bool gethostname(char *buf, size_t bufsize) {
    return OS.gethostname(buf, bufsize) == 0;
}

// Puts the current working directory's path into the buffer.
// Returns false if the buffer is too small.
pub bool getcwd(char *buf, size_t n) {
    return OS.getcwd(buf, n) != NULL;
}

// Returns the pointer to env variable with the specified name
// or NULL if there is no match.
pub const char *getenv(const char *name) {
	return OS.getenv(name);
}
