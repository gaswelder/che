import "prng/neumann"
import "opt"

int main(int argc, char **argv)
{
	int n = 100;
	opt(OPT_INT, "n", "Number of values to generate", &n);
	opt_summary("hw [-n num]");

	argv = opt_parse(argc, argv);
	if(*argv) {
		fprintf(stderr, "Unknown argument: %s\n", *argv);
		return 1;
	}

	if(n < 0) {
		fprintf(stderr, "Negative number\n");
		return 1;
	}

	int16_t x = (int16_t)(time(NULL));
	int i;
	for(i = 0; i < n; i++) {
		x = nrg_next(x);
		printf("%d\n", x);
	}
}
