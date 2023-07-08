// #define _XOPEN_SOURCE 700
#include <unistd.h>
#include <sys/wait.h>

#import os/io

pub typedef {
    io.handle_t *stdin, *stdout, *stderr;
    int pid;
} proc_t;

/*
 * Executes the program with given args and environment.
 * argv is a NULL-terminated list of strings, where argv[0] is the program path.
 * env is a NULL-terminated list of k=v strings specifying environment variables.
 */
pub proc_t *spawn(char *argv[], *env[]) {
    // Make three pipes
    int in[2] = {0};
    int out[2] = {0};
    int err[2] = {0};
    bool ok1 = OS.pipe(in) == 0;
    bool ok2 = OS.pipe(out) == 0;
    bool ok3 = OS.pipe(err) == 0;
    if (!ok1 || !ok2 || !ok3) {
        if (ok1) {
            OS.close(in[0]); OS.close(in[1]);
        }
        if (ok2) {
            OS.close(out[0]); OS.close(out[1]);
        }
        if (ok3) {
            OS.close(err[0]); OS.close(err[1]);
        }
        return NULL;
    }

    int pid = OS.fork();
    if (pid < 0) {
        OS.close(in[0]); OS.close(in[1]);
        OS.close(out[0]); OS.close(out[1]);
        OS.close(err[0]); OS.close(err[1]);
        return NULL;
    }

    if (pid > 0) {
        // Parent
        proc_t *p = calloc(1, sizeof(proc_t));
        if (!p) {
            // oopsie
            OS.close(in[0]); OS.close(in[1]);
            OS.close(out[0]); OS.close(out[1]);
            OS.close(err[0]); OS.close(err[1]);
            OS.kill(pid, OS.SIGKILL);
            return NULL;
        }
        p->pid = pid;
        p->stdin = io.fdhandle(in[0]);
        p->stdout = io.fdhandle(out[0]);
        p->stderr = io.fdhandle(err[0]);
        OS.close(in[1]);
        OS.close(out[1]);
        OS.close(err[1]);
        return p;
    } else {
        // child proc
        OS.close(in[0]);
        OS.close(out[0]);
        OS.close(err[0]);
        if (OS.dup2(in[1], OS.STDIN_FILENO) < 0) {
            panic("dup2 failed");
        }
        if (OS.dup2(out[1], OS.STDOUT_FILENO) < 0) {
            panic("dup2 failed");
        }
        if (OS.dup2(err[1], OS.STDERR_FILENO) < 0) {
            panic("dup2 failed");
        }
        OS.execve(argv[0], argv, env);
        // Shouldn't reach here.
        panic("execve '%s' failed: %s\n", argv[0], strerror(errno));
    }
}

pub bool wait(proc_t *p, int *status) {
    bool ok = OS.waitpid(p->pid, status, 0) == p->pid;
    free(p);
    return ok;
}
