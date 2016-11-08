
struct mem {
	char *data;
	size_t pos; // current position, <= datalen
	size_t datalen; // actual data size, <= size
	size_t size; // allocated buffer size
};

typedef struct mem MEM;

/*
 * Creates a memory buffer.
 * 'name' and 'mode' are ignored.
 */
pub MEM *memopen(const char *name, const char *mode) {
	(void) name;
	(void) mode;
	MEM *m = calloc( 1, sizeof(struct mem) );
	return m;
}

/*
 * Closes the buffer.
 */
pub void memclose(MEM *mem) {
	if(mem->data) {
		free(mem->data);
	}
	free( mem );
}

/*
 * Write 'count' pieces of size 'size' from the buffer 'buf'.
 */
pub size_t memwrite(const void *buf, size_t size, size_t count, MEM *m)
{
	if(count >= SIZE_MAX/size) {
		puts("Size overflow");
		return 0;
	}

	size_t total = size * count;

	if( !makespace(m, total) ) {
		puts("No memory");
		return 0;
	}

	size_t i;
	char *p = (char *) buf;
	for(i = 0; i < total; i++) {
		m->data[m->pos++] = *p++;
	}

	/*
	 * Update the datalen. We do it this way because
	 * 'pos' could be reset far below the data size.
	 */
	if(m->pos > m->datalen) {
		m->datalen = m->pos;
	}
	return total;
}

/*
 * Read up to 'count' pieces of size 'size' to the buffer 'buf'.
 * Returns the number of pieces read.
 */
pub size_t memread(void *buf, size_t size, size_t count, MEM *m)
{
	size_t i;
	char *p = (char *) buf;
	for(i = 0; i < count; i++)
	{
		if(m->pos + size > m->datalen) {
			break;
		}
		size_t j;
		for(j = 0; j < size; j++) {
			*p++ = m->data[m->pos++];
		}
	}
	return i;
}

/*
 * Resets the position to zero.
 */
pub void memrewind(MEM *m) {
	m->pos = 0;
}

/*
 * Returns 1 if the following call to memgetc will return EOF
 */
pub int meof(MEM *m) {
	return m->pos >= m->datalen;
}

/*
 * Returns next character of EOF.
 */
pub int memgetc(MEM *m) {
	if(m->pos >= m->datalen) {
		return EOF;
	}
	int c = m->data[m->pos];
	m->pos++;
	return c;
}

/*
 * Makes sure there is space for data of the additional given size.
 * Returns 0 if there is no space and additional memory couldn't be
 * allocated.
 */
int makespace(MEM *mem, size_t size)
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
