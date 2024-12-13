#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>
#include <stdlib.h>

#type fd_set

typedef struct timeval timeval_t;

pub bool stdin_has_input() {
    fd_set readfds = {0};
    OS.FD_ZERO(&readfds);
    OS.FD_SET(OS.STDIN_FILENO, &readfds);
    timeval_t timeout = {0};
    return OS.select(1, &readfds, NULL, NULL, &timeout) != 0;
}

pub bool get_hostname(char *buf, size_t bufsize) {
    return OS.gethostname(buf, bufsize) == 0;
}

pub bool getcwd(char *buf, size_t n) {
    return OS.getcwd(buf, n) != NULL;
}

pub const char *getenv(const char *name) {
	return OS.getenv(name);
}
