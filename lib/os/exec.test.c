#import os/exec
#import test
#import os/io

int main() {
	char *argv[] = {"/bin/echo", "this is a test from /bin/echo", NULL};
    char *env[] = {NULL};
    exec.proc_t *p = exec.spawn(argv, env);
    if (!p) {
        fprintf(stderr, "spawn failed: %s\n", strerror(errno));
        return 1;
    }
    io.close(p->stdin);

    io.buf_t *b = io.newbuf();
    while (true) {
        if (!io.read(p->stdout, b)) {
            fprintf(stderr, "read failed: %s\n", strerror(errno));
            break;
        }
        size_t n = io.bufsize(b);
        if (n == 0) {
            break;
        }
        test.streq(b->data, "this is a test from /bin/echo\n");
        io.shift(b, n);
    }

    int status = 0;
    if (!exec.wait(p, &status)) {
        fprintf(stderr, "wait failed: %s\n", strerror(errno));
        return 1;
    }
    test.truth("status == 0", status == 0);
    test.truth("errno == 0", errno == 0);
    return test.fails();
}
