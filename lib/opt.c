/*
 * Twice as much as a borderline sane program would need.
 */
#define MAX_FLAGS 32

/*
 * Option value types for use in the 'opt' function
 */
enum {
	OPT_STR = 's',
	OPT_INT = 'i',
	OPT_UINT = 'u',
	OPT_BOOL = 'b',
	OPT_SIZE = 'z'
};

/*
 * A structure describing an option.
 */
struct optspec {
	int type;
	const char *name;
	const char *desc;
	void *value_pointer;
};

/*
 * List of defined options.
 */
struct optspec specs[MAX_FLAGS];
int flags_num = 0;

const char *progname = "progname";
const char *summary = NULL;

/*
 * Declares an option.
 * 'type' is one of 'OPT_...' constants.
 * 'name' is the flag name, as it appears in a command line.
 * 'desc' is text description.
 * 'value_pointer' is a pointer to save the flag value at.
 */
pub void opt(int type, const char *name, const char *desc, void *value_pointer)
{
	if( strlen(name) > 1 ) {
		fprintf(stderr, "Only one-letter flags supported\n" );
		exit(1);
	}

	if( flags_num >= MAX_FLAGS ) {
		fprintf( stderr, "Reached flags limit (%d)\n", MAX_FLAGS );
		exit(1);
	}

	switch(type)
	{
		case OPT_BOOL:
			*( (int *)value_pointer) = 0;
			break;

		case OPT_INT:
		case OPT_UINT:
		case OPT_STR:
		case OPT_SIZE:
			break;

		default:
			fprintf(stderr, "Unknown opt type");
			exit(1);
	}

	specs[flags_num].type = type;
	specs[flags_num].name = name;
	specs[flags_num].desc = desc;
	specs[flags_num].value_pointer = value_pointer;
	flags_num++;
}

/*
 * Parses the given arguments using the information defined
 * with previous 'opt' function calls.
 */
pub char **opt_parse( int argc, char **argv )
{
	opterr = 0;
	progname = argv[0];
	char optstr[MAX_FLAGS * 2 + 2];

	char *p = optstr;
	*p++ = ':';
	int i;
	for( i = 0; i < flags_num; i++ )
	{
		*p++ = specs[i].name[0];
		if( specs[i].type != OPT_BOOL ) {
			*p++ = ':';
		}
	}
	*p = '\0';

	int c;
	while( (c = getopt(argc, argv, optstr)) != -1 )
	{
		if( c == ':' ) {
			fprintf( stderr, "'%c' flag requires an argument\n", optopt );
			opt_usage();
			exit(1);
		}

		if( c == '?' ) {
			fprintf(stderr, "Unknown flag: '%c'\n", optopt );
			opt_usage();
			exit(1);
		}

		/*
		 * Find the flag.
		 */
		int pos = -1;
		for(i = 0; i < flags_num; i++) {
			if( specs[i].name[0] == c ) {
				pos = i;
				break;
			}
		}
		if( pos == -1 ) {
			fprintf(stderr, "Couldn't find spec for flag %c\n", c);
			exit(1);
		}

		struct optspec *flag = &specs[pos];

		switch( flag->type )
		{
			case OPT_BOOL:
				*( (int *) flag->value_pointer ) = 1;
				break;
			case OPT_STR:
				*( (char **) flag->value_pointer ) = optarg;
				break;
			case OPT_INT:
			case OPT_UINT:
			case OPT_SIZE:
				if( !is_numeric( optarg ) ) {
					fprintf(stderr, "Option %c expects a numeric argument\n", c );
					exit(1);
				}
				if( flag->type == OPT_UINT || flag->type == OPT_SIZE )
				{
					if( optarg[0] == '-' ) {
						fprintf(stderr, "Option %c expects a non-negative argument\n", c );
						exit(1);
					}
				}

				if( flag->type == OPT_UINT )
				{
					unsigned val;
					if( sscanf(optarg, "%u", &val) < 1 ) {
						fprintf(stderr, "Couldn't parse value");
						exit(1);
					}
					*( (unsigned *) flag->value_pointer ) = val;
				}
				else if( flag->type == OPT_SIZE )
				{
					size_t val;
					if( sscanf(optarg, "%lu", &val) < 1 ) {
						fprintf(stderr, "Couldn't parse value");
						exit(1);
					}
					*( (size_t *) flag->value_pointer ) = val;
				}
				else
				{
					int val;
					if( sscanf(optarg, "%d", &val) < 1 ) {
						fprintf(stderr, "Couldn't parse value");
						exit(1);
					}
					*( (int *) flag->value_pointer ) = val;
				}

				break;
			default:
				fprintf(stderr, "Unhandled flag type: %c\n", flag->type);
				exit(1);
		}
	}

	argv += optind;
	return argv;
}

pub void opt_summary(const char *s) {
	summary = s;
}

pub void opt_usage(void)
{
	if( summary ) {
		fprintf( stderr, "Usage: %s\n", summary );
	} else {
		fprintf( stderr, "Usage: %s [options] [arguments]\n", progname );
	}

	int i;
	for(i = 0; i < flags_num; i++)
	{
		fprintf( stderr, "\t-%s", specs[i].name );
		switch( specs[i].type )
		{
			case OPT_STR:
				fprintf( stderr, " string" );
				break;
			case OPT_INT:
				fprintf( stderr, " integer" );
				break;
			case OPT_UINT:
				fprintf( stderr, " integer > 0" );
				break;
		}
		fprintf( stderr, "\t%s\n", specs[i].desc );
	}
}

int is_numeric(const char *s)
{
	const char *p = s;

	if( *p == '-' ) p++;
	while( *p != '\0' )
	{
		if( !isdigit(*p) ) {
			return 0;
		}
		p++;
	}
	return 1;
}
