import "prng/neumann"
import "opt"
import "cli"

int main(int argc, char **argv)
{
	int n = 100;
	opt(OPT_INT, "n", "Number of values to generate", &n);
	opt_summary("hw [-n num]");

	argv = opt_parse(argc, argv);
	if(*argv) {
		fatal("Unknown argument: %s", *argv);
	}

	if(n < 0) {
		fatal("Negative number");
	}

	int16_t x = (int16_t)(time(NULL));
	int i;
	for(i = 0; i < n; i++) {
		x = nrg_next(x);
		printf("%d\n", x);
	}
}
