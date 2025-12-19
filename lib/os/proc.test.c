#import os/proc
#import test
#import reader
#import writer

int main() {
	testcat();
	testecho();
    return test.fails();
}

void testcat() {
	char *argv[] = {"/bin/cat", NULL};
    char *env[] = {NULL};
    proc.proc_t *p = proc.spawn(argv, env);
    if (!p) {
        fprintf(stderr, "spawn failed: %s\n", strerror(errno));
        exit(1);
    }

	int r = writer.write(p->stdin, (uint8_t*)"foo\n", 4);
	test.truth("nwrite == 4", r == 4);
	writer.free(p->stdin);

	uint8_t buf[4096] = {};
	reader.read(p->stdout, buf, sizeof(buf));
	test.streq((char*) buf, "foo\n");
}

void testecho() {
	// Spawn the process.
	char *argv[] = {"/bin/echo", "this is a test from /bin/echo", NULL};
    char *env[] = {NULL};
    proc.proc_t *p = proc.spawn(argv, env);
    if (!p) {
        fprintf(stderr, "spawn failed: %s\n", strerror(errno));
        exit(1);
    }

	// Close its input.
    writer.free(p->stdin);

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
    if (!proc.wait(p, &status)) {
        fprintf(stderr, "wait failed: %s\n", strerror(errno));
        exit(1);
    }
    test.truth("status == 0", status == 0);
    test.truth("errno == 0", errno == 0);
}
