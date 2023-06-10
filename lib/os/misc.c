#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>

#type fd_set
#known FD_ZERO
#known FD_SET
#known select
#known gettimeofday
#known tcgetattr
#known tcsetattr
#known STDIN_FILENO

pub typedef struct timeval timeval_t;

pub bool stdin_has_input() {
    fd_set readfds = {0};
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    timeval_t timeout = {0};
    return select(1, &readfds, NULL, NULL, &timeout) != 0;
}

/*
 * Returns current system time as an opaque object.
 */
pub timeval_t get_time() {
    timeval_t time;
    gettimeofday(&time,0);
    return time;
}

/*
 * Returns unix timestamp in microseconds for the given time object.
 */
pub int32_t time_usec(timeval_t *t) {
    return t->tv_sec * 1000000L + t->tv_usec;
}
