#import mt
#import opt
#import cli

int main(int argc, char **argv)
{
	size_t n = 10;
	opt.size("n", "Number of values to generate", &n);
	opt.summary("rand [-n num]");

	argv = opt.parse(argc, argv);
	if(*argv) {
		fatal("Unknown argument: %s", *argv);
	}

	mt.seed( (uint32_t) time(NULL) );
	while(n-- > 0) {
		printf("%f\n", mt.randf());
	}
}
