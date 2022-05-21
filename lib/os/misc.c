#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>

typedef struct timeval _timeval;

pub bool stdin_has_input() {
    fd_set readfds = {0};
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    _timeval timeout = {0};
    return select(1, &readfds, NULL, NULL, &timeout) != 0;
}
