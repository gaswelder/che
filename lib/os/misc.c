#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>
#include <stdlib.h>

#type fd_set
#known FD_ZERO
#known FD_SET
#known select
#known gettimeofday
#known tcgetattr
#known tcsetattr
#known STDIN_FILENO

typedef struct timeval timeval_t;

pub bool stdin_has_input() {
    fd_set readfds = {0};
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    timeval_t timeout = {0};
    return select(1, &readfds, NULL, NULL, &timeout) != 0;
}

pub bool setenv(const char *name, *value, int overwrite) {
    return OS.setenv(name, value, overwrite) == 0;
}

pub bool get_hostname(char *buf, size_t bufsize) {
    return OS.gethostname(buf, bufsize) == 0;
}

pub bool clearenv() {
    fprintf(stderr, "TODO: clearenv\n");
    return false;
}

pub bool getcwd(char *buf, size_t n) {
    return OS.getcwd(buf, n) != NULL;
}
