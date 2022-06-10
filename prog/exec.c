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
    printf("err = %s, exit status = %d\n", strerror(errno), status);
}
