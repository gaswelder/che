import "zio"
import "opt"
import "os/dir"
import "fileutil"
import "strutil"
import "cli"

enum {
	L_SAME,
	L_UNIX,
	L_WIN,
	/*
	 * L_NONE may occur at the end of file
	 */
	L_NONE
};

/*
 * Line-ending format
 */
int lf = L_SAME;

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
			//fprintf( stderr, "trim: skipping directory %s\n", *path );
			path++;
			continue;
		}
		if( !trim_file( *path ) ) {
			return 1;
		}
		path++;
	}
	return 0;
}

/*
 * We read lines one by one and for each line mark three parameters:
 * 1) 'last_pos' - position of last non-space character in the line;
 * 2) 'eol_pos' - position where the line ends (where EOF, '\n' or
 *    '\r' appears);
 * 3) type of end of line (CRLF, LF or EOF).

 * If there are no non-space characters, 'last_pos' is -1.
 * A line needs trimming if 'last_pos' + 1 < 'eol_pos'.
 */

struct linebuf {
	char *line;
	size_t maxsize;
	int eol_pos;
	int last_pos;
	int lf;
	int num;
};

bool trim_file(const char *path)
{
	FILE *f = fopen(path, "rb");
	if(!f) return false;

	zio *out = zopen("mem", "", "");
	if(!out) fatal("Out of memory");

	bool changed = trim(f, out, path);
	fclose(f);

	if(changed) {
		puts(path);
		save(out, path);
	}
	return true;
}

bool trim(FILE *in, zio *out, const char *fpath)
{
	struct linebuf buf = {0};

	bool changed = false;
	while(1) {
		/*
		 * Read line from the input
		 */
		if(!read_line(in, &buf, fpath)) break;

		/*
		 * If the line needs trimming, we just move eol_pos to
		 * the "correct" position.
		 */
		if(buf.last_pos + 1 < buf.eol_pos) {
			buf.eol_pos = buf.last_pos + 1;
			changed = true;
		}

		/*
		 * Override EOL if necessary.
		 */
		if(lf != L_SAME && buf.lf != lf) {
			changed = true;
			buf.lf = lf;
		}

		/*
		 * Write the resulting line out
		 */
		write_line(&buf, out);
	}

	if(buf.line) free(buf.line);
	return changed;
}

/*
 * Reads a line into the line buffer.
 * Returns false if the stream is empty.
 */
bool read_line(FILE *in, struct linebuf *buf, const char *fpath)
{
	if(fpeek(in) == EOF) return false;

	buf->eol_pos = -1;
	buf->last_pos = -1;
	buf->lf = L_NONE;

	int len = 0;

	while(1) {
		int c = getc(in);
		if(c == EOF) {
			break;
		}

		if(c == '\r') {
			if(fpeek(in) != '\n') {
				if(feof(in)) {
					err("WTF! %c", fgetc(in));
				}
				err("%s: unknown eol sequence at line %d: (%d,%d)",
					fpath, buf->num, c, fpeek(in));
				return false;
			}
			getc(in);
			buf->lf = L_WIN;
			break;
		}

		if(c == '\n') {
			buf->lf = L_UNIX;
			break;
		}

		if(!isspace(c)) {
			buf->last_pos = len;
		}

		if((size_t) len >= buf->maxsize && !growbuf(buf)) {
			fatal("Couldn't allocate > %zu bytes for line", buf->maxsize);
		}
		buf->line[len] = c;
		len++;
	}

	buf->eol_pos = len;
	buf->num++;
	return true;
}

bool growbuf(struct linebuf *buf)
{
	size_t newsize = buf->maxsize;
	if(!newsize) newsize = 4096;
	else newsize *= 2;

	void *r = realloc(buf->line, newsize);
	if(!r) return false;
	buf->maxsize = newsize;
	buf->line = r;
	return true;
}

void write_line(struct linebuf *buf, zio *out)
{
	for(int i = 0; i < buf->eol_pos; i++) {
		zputc(buf->line[i], out);
	}
	switch(buf->lf) {
		case L_UNIX:
			zputc('\n', out);
			break;
		case L_WIN:
			assert(zputc('\r', out) != EOF);
			assert(zputc('\n', out) != EOF);
			break;
		default:
			fatal("write_line: unhandled eol type: %d", buf->lf);
	}
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
		if(ch == EOF) {
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
