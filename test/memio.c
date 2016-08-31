// License: MIT (https://opensource.org/licenses/MIT)

struct mem {
	size_t size;
	size_t pos;
	char *data;
	size_t datalen;
	int eof;
};

static int makespace(MEM *mem, size_t size);

MEM *mopen() {
	MEM *m = calloc( 1, sizeof(*m) );
	return m;
}

void mclose( MEM *mem ) {
	free( mem->data );
	free( mem );
}

int mputc( int c, MEM *mem )
{
	if( !makespace(mem, 1) ) {
		return EOF;
	}

	mem->data[mem->pos] = (unsigned char) c;
	mem->pos++;
	mem->datalen++;
	mem->eof = 0;
	return c;
}

int mgetc( MEM *mem ) {
	if( mem->pos >= mem->datalen ) {
		mem->eof = 1;
		return EOF;
	}
	return mem->data[mem->pos++];
}

int mputs( const char *str, MEM *mem ) {
	size_t len = strlen( str );
	if( !makespace( mem, len ) ) {
		return EOF;
	}

	const char *p = str;
	while( *p ) {
		mem->data[mem->pos] = (unsigned char) *p;
		p++;
		mem->pos++;
	}
	mem->datalen += len;
	mem->eof = 0;
	return 1;
}

void mrewind( MEM *mem ) {
	mem->pos = 0;
	mem->eof = 0;
}

int meof( MEM *m ) {
	return m->eof;
}

/*
 * Makes sure there is space for data of the additional given size.
 * Returns 0 if there is no space and additional memory couldn't be
 * allocated.
 */
static int makespace(MEM *mem, size_t size)
{
	size_t need = mem->pos + size;

	if( need <= mem->size ) {
		return 1;
	}

	size_t next = mem->size;
	if( !next ) next = 32;
	while( next < need ) {
		next *= 2;
	}

	char *tmp = realloc(mem->data, next);
	if( !tmp ) {
		fprintf( stderr, "realloc failed\n" );
		return 0;
	}

	mem->data = tmp;
	mem->size = next;
	return 1;
}
