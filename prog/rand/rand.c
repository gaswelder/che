#import rnd
#import opt

int main(int argc, char **argv) {
	size_t n = 1;
	size_t len = 16;
	size_t seed = (size_t) time(NULL);
	char *a = "an";
	bool help = false;
	opt.opt_size("n", "number of values to generate", &n);
	opt.opt_size("l", "string length", &len);
	opt.opt_size("s", "seed value", &seed);
	opt.opt_str("a", "alphabet (n is numberic, an is alphanumeric)", &a);
	opt.opt_bool("h", "show help", &help);
	opt.opt_parse(argc, argv);

	if (help) {
		opt.opt_usage();
		return 1;
	}

	const char *alpha;
	if (!strcmp(a, "an")) {
		alpha = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
	} else if (!strcmp(a, "n")) {
		alpha = "0123456789";
	} else {
		fprintf(stderr, "unknown alphabet name: %s\n", a);
		return 1;
	}
	size_t alphasize = strlen(alpha);

	rnd.seed((uint64_t) seed);

	while (n-- > 0) {
		for (size_t i = 0; i < len; i++) {
			putchar(alpha[rnd.u32() % alphasize]);
		}
		putchar('\n');
	}
	return 0;
}
