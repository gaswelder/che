/*
 * Option value types for use in the 'opt' function
 */
enum {
	OPT_STR = 's',
	OPT_INT = 'i',
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
void declare(int type, const char *name, const char *desc, void *value_pointer) {
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

pub void str(const char *name, *desc, char **pointer) {
	declare(OPT_STR, name, desc, pointer);
}
pub void opt_int(const char *name, *desc, int *pointer) {
	declare(OPT_INT, name, desc, pointer);
}
pub void opt_bool(const char *name, *desc, bool *pointer) {
	declare(OPT_BOOL, name, desc, pointer);
}
pub void size(const char *name, *desc, size_t *pointer) {
	declare(OPT_SIZE, name, desc, pointer);
}
pub void opt_float(const char *name, *desc, float *pointer) {
	declare(OPT_FLOAT, name, desc, pointer);
}

/*
 * Parses the given arguments using the information defined with previous 'opt'
 * function calls.
 */
pub char **opt_parse( int argc, char **argv )
{
	(void) argc;
	// Set global prognme from the given args vector.
	progname = argv[0];

	char **arg = argv + 1;
	while (*arg) {
		if (!strcmp(*arg, "-") || *arg[0] != '-') {
			break;
		}

		// Group of flags (-abc) - must be a run of booleans.
		if (strlen(*arg) > 2) {
			for (char *c = *arg + 1; *c; c++) {
				optspec_t *flag = find(*c);
				if (!flag) {
					fprintf(stderr, "unknown flag: '%c'\n", *c);
					exit(1);
				}
				if (flag->type != OPT_BOOL) {
					fprintf(stderr, "flag %c requires a value\n", *c);
					exit(1);
				}
				*((bool*) flag->value_pointer) = true;
			}
			arg++;
			continue;
		}

		// Single flag (-k)
		if (strlen(*arg) == 2) {
			char *d = *arg + 1;
			char c = *d;
			optspec_t *flag = find(c);
			if (!flag) {
				fprintf(stderr, "unknown flag: %c\n", c);
				exit(1);
			}
			if (flag->type == OPT_BOOL) {
				*((bool*) flag->value_pointer) = true;
				arg++;
				continue;
			}

			arg++;
			if (!arg) {
				fprintf(stderr, "%c flag requires an argument\n", c);
				exit(1);
			}

			switch (flag->type) {
				case OPT_STR: {
					*( (char **) flag->value_pointer ) = *arg;
				}

				case OPT_INT, OPT_SIZE: {
					if (!is_numeric(*arg)) {
						fprintf(stderr, "Option %c expects a numeric argument\n", c);
						exit(1);
					}
					if (flag->type == OPT_SIZE && *arg[0] == '-') {
						fprintf(stderr, "Option %c expects a non-negative argument\n", c );
						exit(1);
					}
					if (flag->type == OPT_SIZE) {
						size_t val = 0;
						if (sscanf(*arg, "%zu", &val) < 1) {
							fprintf(stderr, "Couldn't parse size value: %s\n", *arg);
							exit(1);
						}
						*( (size_t *) flag->value_pointer ) = val;
					}
					else {
						int val = 0;
						if (sscanf(*arg, "%d", &val) < 1) {
							fprintf(stderr, "Couldn't parse value");
							exit(1);
						}
						int *p = flag->value_pointer;
						*p = val;
					}
				}

				case OPT_FLOAT: {
					float val = 0;
					if (sscanf(*arg, "%f", &val) < 1) {
						fprintf(stderr, "Couldn't parse value for flag '%c': %s", c, *arg);
						exit(1);
					}
					*( (float *) flag->value_pointer ) = val;
				}

				default: {
					fprintf(stderr, "Unhandled flag type: %c\n", flag->type);
					exit(1);
				}
			}
			arg++;
			continue;
		}
		fprintf(stderr, "couldn't parse flag group: %s\n", *arg);
		exit(1);
	}
	return arg;
}

optspec_t *find(int c) {
	for(int i = 0; i < flags_num; i++) {
		if( specs[i].name[0] == c ) {
			return &specs[i];
		}
	}
	return NULL;
}

pub void opt_summary(const char *s) {
	summary = s;
}

/*
 * Prints a usage string generated from the flags to stderr.
 * Returns 1 so it can be used as a return status for main.
 */
pub int usage() {
	if (summary) {
		fprintf(stderr, "%s\n", summary);
	}
	fprintf(stderr, "Options:\n");
	for (int i = 0; i < flags_num; i++) {
		optspec_t *s = &specs[i];
		fprintf(stderr, "\t-%s", s->name);
		switch (s->type) {
			case OPT_STR: {
				char **val = s->value_pointer;
				fprintf(stderr, " str");
				fprintf(stderr, "\t%s", s->desc);
				fprintf(stderr, " (= %s)\n", *val);
			}
			case OPT_INT: {
				fprintf(stderr, " int \t%s (%d)\n", s->desc, *((int*) s->value_pointer));
			}
			case OPT_SIZE: {
				fprintf( stderr, "\t%s (%zu)\n", s->desc, *((size_t *)s->value_pointer));
			}
			default: {
				fprintf( stderr, "\t%s\n", s->desc );
			}
		}
	}
	return 1;
}

bool is_numeric(const char *s) {
	const char *p = s;
	if (*p == '-') p++;
	while (*p) {
		if (!isdigit(*p)) return false;
		p++;
	}
	return true;
}
