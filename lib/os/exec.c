#define _XOPEN_SOURCE 700
#include <unistd.h>
#include <sys/wait.h>

pub typedef { int pid; char *error; } exec_t;
pub typedef { FILE *read, *write; } pipe_t;

/*
 * Creates a pipe for inter-process communication. One side of the pipe can be
 * passed as a stream to the `exec` function. Don't forget to close the passed
 * side of the pipe on the caller side to avoid deadlocks.
 */
pub pipe_t exec_makepipe() {
    int fd[2] = {0};
    pipe_t r = { .read = NULL, .write = NULL };
    if (pipe(fd) == -1) {
        return r;
    }
    r.read = fdopen(fd[0], "rb");
    r.write = fdopen(fd[1], "wb");
    return r;
}

/*
 * Executes the program with given args and environment. argv[0] is the program
 * file path.
 * `in`, `out` and `err` specify standard streams. Pass `stdin`, `stdout` and
 * `stderr` for usual terminal output.
 */
pub exec_t *exec(char *argv[], *env[], FILE *in, *out, *err) {
    exec_t *r = calloc(1, sizeof(exec_t));
    if (!r) {
        return NULL;
    }

    r->pid = fork();
    if (r->pid == -1) {
        r->error = strerror(errno);
        return r;
    }
    if (r->pid) {
        return r;
    }

    free(r);
    child(in, out, err, argv, env);
    exit(123);
}

pub bool exec_wait(exec_t *r, int *status) {
    bool ok = waitpid(r->pid, status, 0) == r->pid;
    free(r);
    return ok;
}

void child(FILE *in, *out, *err, char **argv, **env) {
    /*
     * Replace standard streams with those given in the arguments.
     */
    if (in != stdin) {
        fclose(stdin);
        if (dup(fileno(in)) < 1) {
            exit(errno);
        }
    }
    if (out != stdout) {
        fclose(stdout);
        if (dup(fileno(out)) < 1) {
            exit(errno);
        }
    }
    if (err != stderr) {
        fclose(stderr);
        if (dup(fileno(err)) < 1) {
            exit(errno);
        }
    }
    int rr = execve(argv[0], argv, env);
    fprintf(err, "execve '%s' failed: %s\n", argv[0], strerror(errno));
    exit(rr);
}
