#import fs
#import opt
#import strings
#import time

#define MAXPATH 255

/*
 * datelog [-p <period>] [-d <dir>] [-c <current file name>] [-o]
 */
int main(int argc, char *argv[])
{
	char *pername = "month";
	char *dir = ".";
	char *static_name = NULL;
	bool output = false;

	/*
	 * Parse the arguments.
	 */
	opt.opt_summary( "datelog [-o] [-p period] [-d dir] [-c current logname]" );
	opt.str("p", "Log period ('month'/'day'/'hour'/'minute'/'second')", &pername);
	opt.str("d", "Directory for log files", &dir);
	opt.str("c", "File name for current log file", &static_name);
	opt.opt_bool("o", "Output received lines to stdout", &output);

	char **args = opt.opt_parse(argc, argv);
	if (!args) return opt.usage();
	if (*args) {
		fprintf(stderr, "no arguments are expected\n");
		return opt.usage();
	}

	dlog_t *log = dlog_init( pername, dir, static_name );
	if( !log ) {
		return 1;
	}

	char buf[BUFSIZ] = {};
	while( fgets(buf, sizeof(buf), stdin) )
	{
		if( output ) {
			fprintf( stdout, "%s", buf );
		}
		if( !dlog_put( log, buf ) ) {
			fprintf( stderr, "Write to a log failed.\n" );
			break;
		}
	}

	dlog_free( log );
	return 0;
}

typedef {
	const char *name;
	const char *format;
} period_t;

period_t periods[] = {
	{ "month", "%Y-%m" },
	{ "day", "%Y-%m-%d" },
	{ "hour", "%Y-%m-%d-%H" },
	{ "minute", "%Y-%m-%d-%H-%M" },
	{ "second", "%Y-%m-%d-%H-%M-%S" }
};

typedef {
	period_t *per;
	char *dir;
	char *static_path;
	char current_path[MAXPATH];
	FILE *fp;
} dlog_t;


dlog_t *dlog_init( const char *pername, const char *dir,
	const char *static_name )
{
	/*
	 * Find the specified period in the list.
	 */
	period_t *per = find_period( pername );
	if( !per ) {
		fprintf(stderr, "Unknown period: %s\n", pername);
		return NULL;
	}

	/*
	 * Check if the directory exists.
	 */
	if( !fs.is_dir( dir ) ) {
		return NULL;
	}

	dlog_t *log = calloc( 1, sizeof(dlog_t) );
	if( !log ) {
		fprintf(stderr, "Memory allocation failed\n");
		return NULL;
	}

	log->per = per;
	if( dir ) {
		log->dir = strings.newstr("%s", dir);
	}
	if( static_name ) {
		log->static_path = create_static_path( log, static_name );
	}
	return log;
}

int dlog_put( dlog_t *log, const char *line )
{
	/*
	 * Switch the output file if needed.
	 */
	switch_file( log );
	if( !log->fp ) {
		return 0;
	}

	if( fputs( line, log->fp ) == EOF ) {
		return 0;
	}

	return 1;
}

void dlog_free( dlog_t *log )
{
	close_log( log );
	free( log->dir );
	free( log->static_path );
	free( log );
}


/*
 * Returns a pointer to the period with the given name or NULL.
 */
period_t *find_period( const char *pername )
{
	int n = (int) nelem(periods);

	for( int i = 0; i < n; i++ ) {
		if( strcmp( periods[i].name, pername ) == 0 ) {
			return &periods[i];
		}
	}
	return NULL;
}



/*
 * Allocates, formats and returns a full path for the current file.
 */
char *create_static_path( dlog_t *log, const char *static_name )
{
	char *static_path = calloc(MAXPATH, 1);
	if (!static_path) {
		fprintf(stderr, "Memory allocation failed.\n");
		return NULL;
	}

	int len = snprintf(static_path, MAXPATH, "%s/%s", log->dir, static_name);
	if( len < 0 || len >= MAXPATH ) {
		fprintf(stderr, "Static path is too long.\n");
		free( static_path );
		return NULL;
	}

	return static_path;
}


int switch_file( dlog_t *log )
{
	/*
	 * Determine the name the current file should have.
	 */
	char new_path[MAXPATH] = {};
	if( !format_path( log, new_path, sizeof(new_path) ) ) {
		fprintf( stderr, "Could not format new path\n" );
		return 0;
	}

	/*
	 * If the file name hasn't changed, return the current file.
	 */
	if( log->fp && strcmp( new_path, log->current_path ) == 0 ) {
		return 1;
	}

	/*
	 * If the name has changed, close previous file and open the
	 * new one.
	 */
	close_log( log );
	strcpy( log->current_path, new_path );

	const char *path = NULL;
	if (log->static_path) {
		path = log->static_path;
	} else {
		path = log->current_path;
	}
	log->fp = fopen( path, "ab+" );
	if( !log->fp ) {
		fprintf( stderr, "Could not open file '%s' for writing\n",
			path );
		return 0;
	}
	/*
	 * Set line-buffering mode.
	 */
	if( setvbuf( log->fp, NULL, _IOLBF, BUFSIZ ) != 0 ) {
		fprintf( stderr, "Could not change buffer mode\n" );
	}
	return 1;
}

/*
 * Closes current file and renames it if needed.
 */
int close_log( dlog_t *log )
{
	if( !log->fp ) {
		return 1;
	}

	fclose( log->fp );
	log->fp = NULL;

	if( !log->static_path ) {
		return 1;
	}

	/*
	 * Move the contents from the temporary path to the corresponding
	 * file.
	 */
	if (fs.file_exists(log->current_path)) {
		if (!fs.append_file(log->current_path, log->static_path)) {
			fprintf(stderr, "Couldn't append %s to %s\n", log->static_path, log->current_path);
			return 0;
		}
		remove( log->static_path );
	}
	else {
		if( rename( log->static_path, log->current_path ) != 0 ) {
			fprintf( stderr, "Couldn't rename %s to %s\n",
				log->static_path, log->current_path );
			return 0;
		}
	}

	return 1;
}

/*
 * Puts path for the current file into the given buffer.
 */
bool format_path( dlog_t *log, char *buf, size_t len ) {
	char name[200] = {};
	if( !time.time_format(name, sizeof(name), log->per->format) ) {
		return false;
	}
	int r = snprintf(buf, len, "%s/%s.log", log->dir, name);
	if( r < 0 || (size_t) r >= len ) {
		return false;
	}

	return true;
}
