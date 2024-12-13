#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>

#type fd_set

typedef struct timeval timeval_t;

pub typedef struct termios _termios_t;

pub typedef {
    _termios_t original_state;
    _termios_t current_state;
    int fileno;
} term_t;

pub term_t *term_get_stdin() {
    term_t *t = calloc(1, sizeof(term_t));
    t->fileno = OS.STDIN_FILENO;
    OS.tcgetattr(OS.STDIN_FILENO, &t->original_state);
    t->current_state = t->original_state;
    return t;
}

pub bool stdin_has_input() {
    fd_set readfds = {0};
    OS.FD_ZERO(&readfds);
    OS.FD_SET(OS.STDIN_FILENO, &readfds);
    timeval_t timeout = {0};
    return OS.select(1, &readfds, NULL, NULL, &timeout) != 0;
}

pub void term_disable_input_buffering(term_t *t) {
    t->current_state.c_lflag &= ~OS.ICANON & ~OS.ECHO;
    OS.tcsetattr(t->fileno, OS.TCSANOW, &t->current_state);
}

pub void term_restore(term_t *t) {
    OS.tcsetattr(t->fileno, OS.TCSANOW, &t->original_state);
    free(t);
}
