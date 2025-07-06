#import rnd
#import opt

const char *alpha = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
const char *nums = "0123456789";
const char *special = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

int main(int argc, char **argv) {
	size_t n = 1;
	size_t len = 16;
	size_t seed = (size_t) time(NULL);
	char *a = "an";
	opt.nargs(0, "");
	opt.summary("Generates random strings.");
	opt.size("n", "number of values to generate", &n);
	opt.size("l", "string length", &len);
	opt.size("s", "seed value", &seed);
	opt.str("a", "alphabet (s is special chars, n is numbers, a is alphabetic)", &a);
	opt.parse(argc, argv);

	bool _a = false;
	bool _n = false;
	bool _s = false;
	for (char *p = a; *p != '\0'; p++) {
		switch (*p) {
			case 'a': { _a = true; }
			case 'n': { _n = true; }
			case 's': { _s = true; }
			default: {
				fprintf(stderr, "unknown chars selector: %c\n", *p);
				return 1;
			}
		}
	}

	char set[200] = {};
	if (_a) strcat(set, alpha);
	if (_n) strcat(set, nums);
	if (_s) strcat(set, special);

	size_t alphasize = strlen(set);
	rnd.seed((uint64_t) seed);
	while (n-- > 0) {
		for (size_t i = 0; i < len; i++) {
			putchar(set[rnd.intn(alphasize)]);
		}
		putchar('\n');
	}
	return 0;
}
