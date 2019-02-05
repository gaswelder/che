
typedef {
	char *data;
	size_t pos; // current position, <= datalen
	size_t datalen; // actual data size, <= size
	size_t size; // allocated buffer size
} mem_t;

/*
 * Creates a memory buffer.
 * 'name' and 'mode' are ignored.
 */
mem_t *memopen(const char *name, const char *mode) {
	(void) name;
	(void) mode;
	mem_t *m = calloc(1, sizeof(mem_t));
	return m;
}

/*
 * Closes the buffer.
 */
void memclose(mem_t *mem) {
	if(mem->data) {
		free(mem->data);
	}
	free( mem );
}

/*
 * Resets the position to zero.
 */
void memrewind(mem_t *m) {
	m->pos = 0;
}

/*
 * Returns current position in the stream
 */
long memtell(mem_t *m) {
	assert(m->pos <= LONG_MAX);
	return (long) m->pos;
}

/*
 * Puts a character in the memory at current position
 */
int memputc(int ch, mem_t *m)
{
	if(ch == EOF) return EOF;

	if(!makespace(m, 1)) {
		puts("No memory");
		return EOF;
	}

	m->data[m->pos] = ch;
	m->pos++;
	/*
	 * Advance datalen if we have written past it.
	 */
	if(m->pos > m->datalen) {
		m->datalen = m->pos;
	}
	return ch;
}

/*
 * Returns next character of EOF.
 */
int memgetc(mem_t *m)
{
	if(m->pos >= m->datalen) {
		return EOF;
	}
	int c = m->data[m->pos];
	m->pos++;
	return c;
}

/*
 * Write 'size' bytes from buffer 'buf'
 * Returns number of bytes written.
 */
size_t memwrite(mem_t *m, const char *buf, size_t size)
{
	if( !makespace(m, size) ) {
		puts("No memory");
		return 0;
	}

	for(size_t i = 0; i < size; i++) {
		m->data[m->pos] = buf[i];
		m->pos++;
	}

	/*
	 * Advance datalen if we have written past it.
	 */
	if(m->pos > m->datalen) {
		m->datalen = m->pos;
	}
	return size;
}

/*
 * Read up to 'size' bytes to the buffer 'buf'.
 * Returns the number of bytes read.
 */
size_t memread(mem_t *m, char *buf, size_t size)
{
	size_t len = m->datalen - m->pos;
	if(len > size) len = size;

	for(size_t i = 0; i < len; i++) {
		buf[i] = m->data[m->pos];
		m->pos++;
	}
	return len;
}

/*
 * Makes sure there is space for additional data of given size.
 * Returns 0 if there is no space and additional memory couldn't be
 * allocated.
 */
int makespace(mem_t *mem, size_t size)
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
