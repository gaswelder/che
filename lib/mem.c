pub typedef {
	char *data;
	size_t pos; // current position, <= datalen
	size_t datalen; // actual data size, <= size
	size_t size; // allocated buffer size
} mem_t;

/*
 * Creates a memory buffer.
 */
pub mem_t *memopen() {
	mem_t *m = calloc(1, sizeof(mem_t));
	return m;
}

/*
 * Closes the buffer.
 */
pub void memclose(mem_t *mem) {
	if(mem->data) {
		free(mem->data);
	}
	free( mem );
}

/*
 * Resets the position to zero.
 */
pub void memrewind(mem_t *m) {
	m->pos = 0;
}

// Returns current position in the stream
pub size_t memtell(mem_t *m) {
	return m->pos;
}

/*
 * Puts a character in the memory at current position.
 * Returns -1 in case of error (check errno).
 */
pub int memputc(int ch, mem_t *m)
{
	if (ch == EOF) {
		return EOF;
	}
	if (!makespace(m, 1)) {
		return -1;
	}

	m->data[m->pos] = ch;
	m->pos++;
	/*
	 * Advance datalen if we have written past it.
	 */
	if (m->pos > m->datalen) {
		m->datalen = m->pos;
	}
	return ch;
}

/*
 * Returns next character of EOF.
 */
pub int memgetc(mem_t *m)
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
pub size_t memwrite(mem_t *m, const char *buf, size_t size)
{
	if( !makespace(m, size) ) {
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

pub int memprintf(mem_t *m, const char *format, ...) {
	int available = (int) (m->size - m->datalen);

	va_list l = {0};
	va_start(l, format);
	int total = vsnprintf(m->data + m->pos, available, format, l);
	va_end(l);

	/*
	 * If there was not enough buffer, allocate more and try again.
	 */
	if (total >= available) {
		if (!makespace(m, total + 1)) {
			return total;
		}
		va_start(l, format);
		total = vsnprintf(m->data + m->pos, available, format, l);
		va_end(l);
	}
	return total;
}

/*
 * Read up to 'size' bytes to the buffer 'buf'.
 * Returns the number of bytes read.
 */
pub size_t memread(mem_t *m, char *buf, size_t size)
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
bool makespace(mem_t *mem, size_t size)
{
	size_t need = mem->pos + size;

	if( need <= mem->size ) {
		return true;
	}

	size_t next = mem->size;
	if( !next ) next = 32;
	while( next < need ) {
		next *= 2;
	}

	char *tmp = realloc(mem->data, next);
	if( !tmp ) {
		return false;
	}

	mem->data = tmp;
	mem->size = next;
	return true;
}
