#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

pub int fork() {
    return OS.fork();
}

pub int wait(int *status) {
    return OS.wait(status);
}
