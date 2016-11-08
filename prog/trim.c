import "zio"
import "opt"
import "os/dir"
import "fileutil"
import "strutil"

enum {
	L_SAME,
	L_UNIX,
	L_WIN
};

int main( int argc, char *argv[] )
{
	const char *line_format = "unix";

	opt_summary( "trim [-l unix/win/same] path..." );
	opt( OPT_STR, "l", "Convert line format ('unix', 'win' or 'same')", &line_format );

	char **path = opt_parse( argc, argv );
	if( !path ) {
		opt_usage();
		return 1;
	}

	int lf;
	if( strcmp( line_format, "same" ) == 0 ) {
		lf = L_SAME;
	}
	else if( strcmp( line_format, "unix" ) == 0 ) {
		lf = L_UNIX;
	}
	else if( strcmp( line_format, "win" ) == 0 ) {
		lf = L_WIN;
	}
	else {
		fprintf( stderr, "Unknown line format: '%s'\n", line_format );
		return 1;
	}

	while( *path )
	{
		if( is_dir( *path ) ) {
			fprintf( stderr, "trim: skipping directory %s\n", *path );
			path++;
			continue;
		}
		if( !trim_file( *path, lf ) ) {
			return 1;
		}
		path++;
	}
	return 0;
}

int trim_file( const char *path, int lf )
{
	FILE *fp = fopen( path, "rb" );
	if( !fp ) {
		fprintf( stderr, "Could not open '%s' for reading\n", path );
		return 0;
	}

	/*
	 * Create a trimmed copy of the file.
	 */
	zio *copy = zopen("mem", "", "");
	if( !copy ) {
		fprintf( stderr, "Could not allocate memory\n" );
		fclose( fp );
		return 0;
	}
	int changed = 0;
	int ok = 1;
	char line[BUFSIZ];
	size_t linenum = 0;
	while( fgets(line, sizeof(line), fp) )
	{
		linenum++;
		ok = 0;
		if( line_torn( line, sizeof line, fp ) ) {
			fprintf( stderr, "'%s': line %zu is too big\n", path, linenum );
			break;
		}

		int r = trim_line( line, copy, lf );
		if( r < 0 ) {
			break;
		}
		if( r > 0 ) changed = 1;
		ok = 1;
	}

	if( ok && changed ) {
		fprintf( stderr, "%s\n", path );
		ok = save( copy, path );
	}

	zclose( copy );
	fclose( fp );
	return ok;
}

int line_torn( const char *line, size_t bufsize, FILE *fp )
{
	size_t len = strlen( line );
	return len == bufsize - 1
		&& line[len-1] != '\n'
		&& fpeek(fp) != EOF;
}

/*
 * Writes trimmed copy of the line the the given stream.
 * Returns -1 on error. On success returns 0 if the line didn't
 * need trimming, 1 if did.
 */
int trim_line( const char *line, zio *out, int lf )
{
	const char *p = line;

	/*
	 * Set linemarker to point to the end-of-line marker in the line.
	 */
	const char *linemarker;
	while( *p ) {
		p++;
	}
	if( p > line && *(p-1) == '\n' ) p--;
	if( p > line && *(p-1) == '\r' ) p--;
	linemarker = p;

	/*
	 * Set contentent to point to the end of the content in the line.
	 */
	const char *contentend;
	while( p > line && isspace(*(p-1)) ) {
		p--;
	}
	contentend = p;

	/*
	 * Print the content line.
	 */
	size_t len = 0;
	for( p = line; p < contentend; p++ )
	{
		if( zputc( (unsigned char) *p, out) == EOF ) {
			fprintf( stderr, "zputc failed\n" );
			return -1;
		}
		len++;
	}

	/*
	 * Print the end of line marker.
	 */
	if( lf == L_UNIX ) {
		linemarker = "\n";
	}
	else if( lf == L_WIN ) {
		linemarker = "\r\n";
	}
	if( zputs(linemarker, out) < 0 ) {
		return -1;
	}
	len += strlen(linemarker);
	return len != strlen( line );
}

int save( zio *mem, const char *path )
{
	FILE *fp = fopen( path, "wb" );
	if( !fp ) {
		fprintf( stderr, "Couldn't open '%s' for writing\n", path );
		return 0;
	}

	zrewind( mem );
	int ok = 1;
	while( 1 ) {
		int ch = zgetc( mem );
		if( zeof( mem ) ) {
			break;
		}
		int r = fputc(ch, fp);
		if( (char) r != ch ) {
			fprintf( stderr, "Character write failed (%d/%c != %d/%c)\n", (char) r, (char) r, ch, ch );
			ok = 0;
			break;
		}
	}

	fclose( fp );
	return ok;
}

int fpeek( FILE *fp ) {
	int c = fgetc( fp );
	if( c != EOF ) {
		ungetc( c, fp );
	}
	return c;
}
