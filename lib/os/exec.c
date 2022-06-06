#define _XOPEN_SOURCE 700
#include <unistd.h>
#include <sys/wait.h>

pub typedef {
    char *error;
    int status;
} exec_result;

/*
 * Executes the program at path `path` with given args and environment.
 * `in`, `out` and `err` specify standard streams. Pass `stdin`, `stdout` and
 * `stderr` for usual terminal output.
 */
pub exec_result exec(char *path, *argv[], *env[], FILE *in, *out, *err) {
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

    /*
     * Replace standard streams with those given in the arguments.
     */
    if (in != stdin) {
        fclose(stdin);
        if (dup(fileno(in)) < 1) {
            r.error = strerror(errno);
            return r;
        }
    }
    if (out != stdout) {
        fclose(stdout);
        if (dup(fileno(out)) < 1) {
            r.error = strerror(errno);
            return r;
        }
    }
    if (err != stderr) {
        fclose(stderr);
        if (dup(fileno(err)) < 1) {
            r.error = strerror(errno);
            return r;
        }
    }
    int rr = execve(path, argv, env);
    fprintf(stderr, "execve '%s' failed: %s\n", path, strerror(errno));
    exit(rr);
}
