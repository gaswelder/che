#import os/exec

bool test() {
    char *argv[] = {"/bin/echo", "this is a test from /bin/echo", NULL};
    char *env[] = {NULL};
    exec.exec_t *r = exec.exec(argv, env, stdin, stdout, stderr);

    int status = 0;
    if (!exec.exec_wait(r, &status)) {
        fprintf(stderr, "wait failed: %s\n", strerror(errno));
        return false;
    }
    if (errno != 0 || status != 0) {
        return false;
    }
    return true;
}


int main() {
	if (test()) {
		return 0;
	}
	return 1;
}