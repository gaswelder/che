#include <sys/types.h>
#include <unistd.h>

#type pid_t

pub pid_t getpid() {
    return OS.getpid();
}
