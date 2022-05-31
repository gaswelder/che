#import prng/mt
#import opt

const char alpha[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";

int main(int argc, char **argv)
{
	size_t n = 1;
	size_t len = 16;
	size_t seed = (size_t) time(NULL);
	opt(OPT_UINT, "n", "number of strings to generate", &n);
	opt(OPT_UINT, "l", "string length", &len);
	opt(OPT_UINT, "s", "seed value", &seed);
	opt_parse(argc, argv);

	mt_seed((uint32_t) seed);

	while (n > 0) {
		n--;
		for (size_t i = 0; i < len; i++) {
			uint32_t pos = mt_rand32() % sizeof(alpha);
			putchar(alpha[pos]);
		}
		putchar('\n');
	}
	return 0;
}
