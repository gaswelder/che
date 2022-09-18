#import opt

int main() {
	char *s = NULL;
	opt.opt(OPT_STR, "s", "some string", &s);
	int i = 0;
	opt.opt(OPT_INT, "i", "some int", &i);
	float f = 0;
	opt.opt(OPT_FLOAT, "f", "some float", &f);
	bool p = false;
	opt.opt(OPT_BOOL, "p", "some bool1", &p);
	bool q = false;
	opt.opt(OPT_BOOL, "q", "some bool2", &q);

	char *argv[] = {"progname", "-s", "foo", "-i", "10", "-pq", "-f", "3.14", "one", "two", NULL};
	int argc = 0;
	while (argv[argc]) {
		argc++;
	}

	printf("&i = %p\n", (void*) &i);

	char **args = opt.parse(argc, argv);
	bool fail = false;
	if (i != 10) {
		fprintf(stderr, "i: %d != %d\n", i, 10);
		fail = true;
	}
	if (strcmp(s, "foo")) {
		fprintf(stderr, "s: %s != %s\n", s, "foo");
		fail = true;
	}
	if (f - 3.14 > 0.000001) {
		fprintf(stderr, "f: %.10f != %.10f\n", f, 3.14);
		fail = true;
	}
	if (!p) {
		fprintf(stderr, "p != true\n");
		fail = true;
	}
	if (!q) {
		fprintf(stderr, "q != true\n");
		fail = true;
	}
	if (strcmp(args[0], "one")) {
		fprintf(stderr, "args[0]: %s != %s\n", args[0], "one");
		fail = true;
	}
	if (strcmp(args[1], "two")) {
		fprintf(stderr, "args[1]: %s != %s\n", args[1], "two");
		fail = true;
	}
	if (args[2] != NULL) {
		fprintf(stderr, "args[2]: %s != NULL\n", args[2]);
		fail = true;
	}
	return fail;
}
