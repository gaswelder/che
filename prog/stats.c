#import cli

int usage()
{
	fprintf(stderr, "Usage: stats [-s] [-a] [-n] [-h]\n"
		"	-s	print sum\n"
		"	-a	print average\n"
		"	-n	print count\n"
		"	-h	print header\n");
	return 1;
}

int main(int argc, char **argv)
{
	if( argc == 1 ) {
		return usage();
	}
	char cols[4] = "";
	int ncols = 0;

	bool header = false;

	argv++;
	while( *argv && *argv[0] == '-' )
	{
		char *arg = *argv++;
		if( strcmp(arg, "-h") == 0) {
			header = true;
			continue;
		}

		arg++;
		if( strlen(arg) != 1 ) {
			fatal("Invalid argument: %s", arg);
		}

		switch (arg[0]) {
			case 's':
			case 'a':
			case 'n':
				for(int i = 0; i < ncols; i++) {
					if(cols[i] == arg[0]) {
						fatal("Duplicate -%c argument", arg[0]);
					}
				}
				cols[ncols++] = arg[0];
				break;
			default:
				fatal("Unknown flag: -%c", arg[0]);
		}
	}

	if( *argv ) {
		err("Unknown argument: %s", *argv);
		return usage();
	}

	if( ncols == 0 ) {
		return usage();
	}

	return process(cols, ncols, header);
}

int process(char *cols, int ncols, bool header)
{
	bool calc_avg = false;
	for(int i = 0; i < ncols; i++) {
		if( cols[i] == 'a' ) {
			calc_avg = true;
			break;
		}
	}

	double sum = 0;
	double avg = 0;
	int num = 0;

	while( 1 )
	{
		double x = 0;
		int r = scanf("%lf\n", &x);
		if( r == EOF ) {
			break;
		}
		if( r != 1 ) {
			fatal("scanf error");
		}

		sum += x;
		num++;
		if( calc_avg ) {
			avg = avg * (num-1)/num + x/num;
		}
	}

	/*
	 * Print the results.
	 */
	if( header )
	{
		char *name = NULL;
		for(int i = 0; i < ncols; i++)
		{
			if(i > 0) {
				putchar('\t');
			}
			switch(cols[i]) {
				case 'a':
					name = "avg";
					break;
				case 's':
					name = "sum";
					break;
				case 'n':
					name = "num";
					break;
				default:
					fatal("Unknown column: %c", cols[i]);
			}

			printf("%s", name);
		}
		putchar('\n');
	}

	for(int i = 0; i < ncols; i++)
	{
		if(i > 0) {
			putchar('\t');
		}
		switch(cols[i]) {
			case 'a':
				printf("%f", avg);
				break;
			case 's':
				printf("%f", sum);
				break;
			case 'n':
				printf("%d", num);
				break;
			default:
				fatal("Unknown column: %c", cols[i]);
		}
	}
	putchar('\n');
	return 0;
}
