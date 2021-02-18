import "net"

enum {
	S_UNKNOWN,
	S_FILE,
	S_MEM,
	S_TCP
};

typedef {
	int type;
	void *h;
} zio;

/*
 * Returns type identifier for given type name.
 */
int gettype(const char *type)
{
	if(strcmp(type, "file") == 0) {
		return S_FILE;
	}
	if(strcmp(type, "mem") == 0) {
		return S_MEM;
	}
	if(strcmp(type, "tcp") == 0) {
		return S_TCP;
	}
	return S_UNKNOWN;
}

pub zio *zopen(const char *type, const char *name, const char *mode)
{
	int t = gettype(type);
	void *h = NULL;

	switch(t) {
		case S_FILE:
			h = fopen(name, mode);
			break;
		case S_MEM:
			h = memopen(name, mode);
			break;
		case S_TCP:
			h = net_conn("tcp", name);
			break;
		default:
			printf("Unknown zio type: %s\n", type);
			return NULL;
	}

	if(!h) {
		printf("zio: open failed\n");
		return NULL;
	}

	zio *s = calloc(1, sizeof(*s));
	s->type = t;
	s->h = h;
	return s;
}

pub void zclose(zio *s)
{
	switch(s->type) {
		case S_FILE:
			fclose((FILE *)s->h);
			break;
		case S_MEM:
			memclose((mem_t *)s->h);
			break;
		case S_TCP:
			net_close(s->h);
			break;
		default:
			puts("Unknown zio type");
	}

	free(s);
}

pub int zread(zio *s, char *buf, int size)
{
	switch(s->type) {
		case S_FILE:
			size_t r = fread(buf, 1, (size_t) size, s->h);
			return (int) r;
		case S_MEM:
			return memread(s->h, buf, (size_t) size);
		case S_TCP:
			return net_read(s->h, buf, (size_t) size);
		default:
			puts("Unknown zio type");
	}
	return -1;
}

pub int zwrite(zio *s, const char *buf, int len)
{
	switch(s->type) {
		case S_FILE:
			return (int) fwrite(buf, 1, (size_t) len, s->h);
		case S_MEM:
			return memwrite(s->h, buf, (size_t) len);
		case S_TCP:
			return net_write(s->h, buf, (size_t) len);
		default:
			puts("Unknown zio type");
	}
	return -1;
}

pub int zputc(int c, zio *s)
{
	if(c == EOF) return EOF;
	if(s->type == S_FILE) {
		return fputc(c, s->h);
	}

	char buf[1] = {};
	buf[0] = (char) c;
	size_t n = 0;

	switch(s->type) {
		case S_MEM:
			return memputc(c, s->h);
			break;
		case S_TCP:
			n = net_write(s->h, buf, sizeof(buf));
			break;
		default:
			puts("Unknown zio type");
	}

	if(n == 1) {
		return c;
	}
	return EOF;
}

pub int zputs(const char *s, zio *z)
{
	if(z->type == S_FILE) {
		return fputs(s, z->h);
	}

	size_t len = strlen(s);

	switch(z->type) {
		case S_MEM:
			return memwrite(z->h, s, len);
		case S_TCP:
			return net_write(z->h, s, len);
		default:
			puts("Unknown zio type");
	}
	return EOF;
}

pub int zrewind(zio *s)
{
	switch(s->type) {
		case S_FILE:
			rewind(s->h);
			return 1;
		case S_MEM:
			memrewind(s->h);
			return 1;
		case S_TCP:
			return 0;
		default:
			puts("Unknown zio type");
	}
	return 0;
}

/*
 * Returns current position in the stream
 * or -1 on error.
 */
pub long ztell(zio *s)
{
	switch(s->type) {
		case S_FILE:
			return ftell(s->h);
		case S_MEM:
			return memtell(s->h);
		case S_TCP:
			return -1;
		default:
			puts("Unknown zio type");
	}
	return -1;
}

pub int zprintf(zio *s, const char *fmt, ...)
{
	va_list args = {};

	/*
	 * Find out how long the string will be
	 */
	va_start(args, fmt);
	int len = vsnprintf(NULL, 0, fmt, args);
	va_end(args);
	if(len < 0) return -1;

	/*
	 * Format the string
	 */
	char *buf = calloc(len + 1, sizeof(char));
	if (!buf) {
		return -1;
	}
	va_start(args, fmt);
	len = vsnprintf(buf, len + 1, fmt, args);
	va_end(args);

	/*
	 * Write the string
	 */
	int n = -1;
	switch(s->type) {
		case S_FILE:
			n = fwrite(buf, 1, len, s->h);
			break;
		case S_MEM:
			n = memwrite(s->h, buf, len);
			break;
		case S_TCP:
			n = net_write(s->h, buf, len);
			break;
		default:
			puts("Unknown zio type");
	}
	free(buf);

	if(n < len) {
		fprintf(stderr, "zprintf write error: len=%d, n=%d\n", len, n);
		exit(1);
	}

	return len;
}

pub int zgetc(zio *s)
{
	switch(s->type) {
		case S_FILE:
			return fgetc(s->h);
		case S_MEM:
			return memgetc(s->h);
		case S_TCP:
			char c = 0;
			int n = net_read(s->h, &c, 1);
			if(n <= 0) {
				return EOF;
			}
			return c;
		default:
			puts("Unknown zio type");
	}
	return EOF;
}



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
