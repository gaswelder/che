#define _XOPEN_SOURCE 700
#include <unistd.h>
#include <sys/wait.h>

pub typedef {
    char *error;
    int status;
} exec_result;

pub exec_result exec(char *path, *argv[], *env[]) {
    exec_result r = { .error = NULL, .status = 0 };

    pid_t pid = fork();
    if (pid == -1) {
        r.error = strerror(errno);
        return r;
    }

    /*
     * Parent
     */
    if (pid) {
        if (waitpid(pid, &r.status, 0) != pid) {
            r.error = strerror(errno);
        }
        return r;
    }
    
    /*
     * Child
     */
    int rr = execve(path, argv, env);
    fprintf(stderr, "execve '%s' failed: %s\n", path, strerror(errno));
    exit(rr);
}
