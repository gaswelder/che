#import os/exec

bool test() {
    char *argv[] = {"/bin/ls", "exec.c", NULL};
    char *env[] = {NULL};
    exec_t *r = exec(argv, env, stdin, stdout, stderr);

    int status = 0;
    if (!exec_wait(r, &status)) {
        fprintf(stderr, "wait failed: %s\n", strerror(errno));
        return false;
    }
    if (errno != 0 || status != 0) {
        return true;
    }
    return false;
}


int main() {
	if (test()) {
        puts("OK os/exec test");
		return 0;
	}
	puts("FAIL os/exec test");
	return 1;
}