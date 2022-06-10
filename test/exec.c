#import os/exec

int main() {
    char *argv[] = {"/bin/ls", "exec.c", NULL};
    char *env[] = {NULL};
    exec_t *r = exec(argv, env, stdin, stdout, stderr);

    int status = 0;
    if (!exec_wait(r, &status)) {
        fprintf(stderr, "wait failed: %s\n", strerror(errno));
        return 1;
    }
    if (errno != 0 || status != 0) {
        return 1;
    }
    return 0;
}
