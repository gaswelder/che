#import prng/mt
#import opt
#import cli

int main(int argc, char **argv)
{
	size_t n = 10;
	opt(OPT_SIZE, "n", "Number of values to generate", &n);
	opt_summary("rand [-n num]");

	argv = opt_parse(argc, argv);
	if(*argv) {
		fatal("Unknown argument: %s", *argv);
	}

	mt_seed( (uint32_t) time(NULL) );
	while(n-- > 0) {
		printf("%f\n", mt_randf());
	}
}
