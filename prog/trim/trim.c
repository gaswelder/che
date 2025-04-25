#import os/fs
#import mem
#import opt

enum {
	L_SAME,
	L_UNIX,
	L_WIN,
	/*
	 * L_NONE may occur at the end of file
	 */
	L_NONE
}

/*
 * Line-ending format
 */
int lf = L_SAME;

int main( int argc, char *argv[]) {
	char *line_format = "unix";

	opt.opt_summary("converts line formats in files");
	opt.str("l", "line format ('unix', 'win' or 'same')", &line_format);

	char **path = opt.parse( argc, argv );
	if (!path) return opt.usage();

	switch str (line_format) {
		case "same": { lf = L_SAME; }
		case "unix": { lf = L_UNIX; }
		case "win": { lf = L_WIN; }
		default: {
			fprintf(stderr, "Unknown line format: '%s'\n", line_format);
			return 1;
		}
	}

	while (*path) {
		if (fs.is_dir(*path)) {
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

typedef {
	char *line;
	size_t maxsize;
	int eol_pos;
	int last_pos;
	int lf;
	int num;
} linebuf_t;

bool trim_file(const char *path)
{
	FILE *f = fopen(path, "rb");
	if(!f) return false;

	mem.mem_t *out = mem.memopen();
	if (!out) {
		panic("Out of memory");
	}

	bool changed = ftrim(f, out, path);
	fclose(f);

	if(changed) {
		puts(path);
		save(out, path);
	}
	return true;
}

bool ftrim(FILE *in, mem.mem_t *out, const char *fpath)
{
	linebuf_t buf = {0};

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
bool read_line(FILE *in, linebuf_t *buf, const char *fpath)
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
					fprintf(stderr, "WTF! %c", fgetc(in));
				}
				fprintf(stderr, "%s: unknown eol sequence at line %d: (%d,%d)",
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
			panic("Couldn't allocate > %zu bytes for line", buf->maxsize);
		}
		buf->line[len] = c;
		len++;
	}

	buf->eol_pos = len;
	buf->num++;
	return true;
}

bool growbuf(linebuf_t *buf)
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

void write_line(linebuf_t *buf, mem.mem_t *out)
{
	for (int i = 0; i < buf->eol_pos; i++) {
		mem.memputc(buf->line[i], out);
	}
	switch (buf->lf) {
		case L_UNIX: {
			mem.memputc('\n', out);
		}
		case L_WIN: {
			assert(mem.memputc('\r', out) != EOF);
			assert(mem.memputc('\n', out) != EOF);
		}
		default: {
			panic("write_line: unhandled eol type: %d", buf->lf);
		}
	}
}

int save(mem.mem_t *mem, const char *path )
{
	FILE *fp = fopen( path, "wb" );
	if( !fp ) {
		fprintf( stderr, "Couldn't open '%s' for writing\n", path );
		return 0;
	}

	mem.memrewind( mem );
	int ok = 1;
	while( 1 ) {
		int ch = mem.memgetc( mem );
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
