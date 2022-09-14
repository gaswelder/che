#define _X_OPEN_SOURCE 700
#include <unistd.h>
/*
 * getopt.h is a GNU quirk, probably. It's not POSIX.
 */
#include <getopt.h>

/*
 * Option value types for use in the 'opt' function
 */
pub enum {
	OPT_STR = 's',
	OPT_INT = 'i',
	OPT_UINT = 'u',
	OPT_BOOL = 'b',
	OPT_SIZE = 'z',
	OPT_FLOAT = 'f'
};

/*
 * List of defined options.
 */
typedef {
	int type;
	const char *name;
	const char *desc;
	void *value_pointer;
} optspec_t;
/*
 * Twice as much as a borderline sane program would need.
 */
const int MAX_FLAGS = 32;
optspec_t specs[32] = {};
int flags_num = 0;

const char *progname = "progname";
const char *summary = NULL;

/*
 * Declares an option.
 * `type` is one of the `OPT_` constants.
 * 'name' is the flag name, as it appears in a command line.
 * 'desc' is text description.
 * 'value_pointer' is a pointer to save the flag value at.
 */
pub void opt_opt(int type, const char *name, const char *desc, void *value_pointer) {
	if (strlen(name) > 1) {
		fprintf(stderr, "Only one-letter flags are supported\n");
		exit(1);
	}
	if (flags_num >= MAX_FLAGS) {
		fprintf(stderr, "Too many flags, max is %d\n", MAX_FLAGS);
		exit(1);
	}
	specs[flags_num].type = type;
	specs[flags_num].name = name;
	specs[flags_num].desc = desc;
	specs[flags_num].value_pointer = value_pointer;
	flags_num++;
}

/*
 * Parses the given arguments using the information defined with previous 'opt'
 * function calls.
 */
pub char **opt_parse( int argc, char **argv )
{
	// Set global prognme from the given args vector.
	progname = argv[0];

	// Compose optstr for getopt
	char *optstr = calloc(1, MAX_FLAGS * 2 + 2);
	char *p = optstr;
	*p++ = ':';
	for (int i = 0; i < flags_num; i++) {
		*p++ = specs[i].name[0];
		if(specs[i].type != OPT_BOOL) {
			*p++ = ':';
		}
	}
	*p = '\0';

	opterr = 0;
	int c = 0;
	while((c = getopt(argc, argv, optstr)) != -1) {
		if(c == ':') {
			fprintf( stderr, "'%c' flag requires an argument\n", optopt );
			opt_usage();
			exit(1);
		}
		if(c == '?') {
			fprintf(stderr, "Unknown flag: '%c'\n", optopt );
			opt_usage();
			exit(1);
		}
		setflag(c);
	}
	argv += optind;
	return argv;
}

void setflag(int c)
{
	/*
	 * Find the flag.
	 */
	int pos = -1;
	for(int i = 0; i < flags_num; i++) {
		if( specs[i].name[0] == c ) {
			pos = i;
			break;
		}
	}
	if( pos == -1 ) {
		fprintf(stderr, "Couldn't find spec for flag %c\n", c);
		exit(1);
	}
	optspec_t *flag = &specs[pos];

	switch( flag->type ) {
		case OPT_BOOL:
			*( (int *) flag->value_pointer ) = 1;
			break;
		case OPT_STR:
			*( (char **) flag->value_pointer ) = optarg;
			break;
		case OPT_INT:
		case OPT_UINT:
		case OPT_SIZE:
			if (!is_numeric( optarg )) {
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
				unsigned val = 0;
				if( sscanf(optarg, "%u", &val) < 1 ) {
					fprintf(stderr, "Couldn't parse value");
					exit(1);
				}
				*( (unsigned *) flag->value_pointer ) = val;
			}
			else if( flag->type == OPT_SIZE )
			{
				size_t val = 0;
				if( sscanf(optarg, "%zu", &val) < 1 ) {
					fprintf(stderr, "Couldn't parse value");
					exit(1);
				}
				*( (size_t *) flag->value_pointer ) = val;
			}
			else
			{
				int val = 0;
				if( sscanf(optarg, "%d", &val) < 1 ) {
					fprintf(stderr, "Couldn't parse value");
					exit(1);
				}
				*( (int *) flag->value_pointer ) = val;
			}

			break;
		case OPT_FLOAT:
			float val = 0;
			if (sscanf(optarg, "%f", &val) < 1) {
				fprintf(stderr, "Couldn't parse value for flag '%c': %s", c, optarg);
				exit(1);
			}
			*( (float *) flag->value_pointer ) = val;
			break;
		default:
			fprintf(stderr, "Unhandled flag type: %c\n", flag->type);
			exit(1);
	}
}

pub void opt_summary(const char *s) {
	summary = s;
}

/*
 * Prints a usage string generated from the flags to stderr.
 */
pub void opt_usage()
{
	if (summary) {
		fprintf(stderr, "%s\n", summary);
	}
	fprintf(stderr, "Options:\n");
	for (int i = 0; i < flags_num; i++) {
		fprintf( stderr, "\t-%s", specs[i].name );
		switch( specs[i].type )
		{
			case OPT_STR:
				fprintf( stderr, " str" );
				break;
			case OPT_INT:
				fprintf( stderr, " int" );
				break;
			case OPT_UINT:
				fprintf( stderr, " int > 0" );
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
