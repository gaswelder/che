
int usage()
{
	fprintf(stderr, "Usage: stat [-s] [-a] [-n] [-h]\n"
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
	char cols[4];
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
			fprintf(stderr, "Invalid argument: %s\n", arg);
			return 1;
		}

		int i;

		switch( arg[0] ) {
			case 's':
			case 'a':
			case 'n':
				for(i = 0; i < ncols; i++) {
					if(cols[i] == arg[0]) {
						fprintf(stderr, "Duplicate -%c argument\n", arg[0]);
						return 1;
					}
				}
				cols[ncols++] = arg[0];
				break;
			default:
				fprintf(stderr, "Unknown flag: -%c\n", arg[0]);
				return 1;
		}
	}

	if( *argv ) {
		fprintf(stderr, "Unknown argument: %s\n", *argv);
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
	int i;
	for(i = 0; i < ncols; i++) {
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
		double x;
		int r = scanf("%lf\n", &x);
		if( r == EOF ) {
			break;
		}
		if( r != 1 ) {
			fprintf(stderr, "scanf error\n");
			exit(1);
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
		char *name;
		for(i = 0; i < ncols; i++)
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
					fprintf(stderr, "Unknown column: %c\n", cols[i]);
					exit(1);
			}

			printf("%s", name);
		}
		putchar('\n');
	}

	for(i = 0; i < ncols; i++)
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
				fprintf(stderr, "Unknown column: %c\n", cols[i]);
				exit(1);
		}
	}
	putchar('\n');
	return 0;
}
