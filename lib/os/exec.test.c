#import os/exec
#import test
#import os/io
#import reader

int main() {
	// Spawn the process.
	char *argv[] = {"/bin/echo", "this is a test from /bin/echo", NULL};
    char *env[] = {NULL};
    exec.proc_t *p = exec.spawn(argv, env);
    if (!p) {
        fprintf(stderr, "spawn failed: %s\n", strerror(errno));
        return 1;
    }

	// Close its input.
    io.close(p->stdin);

	// Read its stdout.
	uint8_t buf[4096] = {};
    while (true) {
		int r = reader.read(p->stdout, buf, sizeof(buf));
		if (r < 0) {
            fprintf(stderr, "read failed: %s\n", strerror(errno));
            break;
        }
		if (r == 0) break;
    }
	test.streq((char *)buf, "this is a test from /bin/echo\n");

    int status = 0;
    if (!exec.wait(p, &status)) {
        fprintf(stderr, "wait failed: %s\n", strerror(errno));
        return 1;
    }
    test.truth("status == 0", status == 0);
    test.truth("errno == 0", errno == 0);
    return test.fails();
}
