import "os/net"

enum {
	S_UNKNOWN,
	S_FILE,
	S_MEM,
	S_TCP
};

struct __zio {
	int type;
	void *h;
};

typedef struct __zio zio;


pub zio *zopen(const char *type, const char *name, const char *mode)
{
	int t = S_UNKNOWN;
	void *h = NULL;

	if(strcmp(type, "file") == 0) {
		t = S_FILE;
		h = fopen(name, mode);
	}
	else if(strcmp(type, "mem") == 0) {
		t = S_MEM;
		h = memopen(name, mode);
	}
	else if(strcmp(type, "tcp") == 0) {
		t = S_TCP;
		h = net_conn("tcp", name);
	}
	else {
		puts("Unknown zio type");
		return NULL;
	}

	if(!h) {
		puts("open failed");
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
			memclose((MEM *)s->h);
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

	char buf[1];
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

pub int zeof(zio *s)
{
	switch(s->type) {
		case S_FILE:
			return feof(s->h);
		case S_MEM:
			return meof(s->h);
		case S_TCP:
			return 0;
	}

	puts("Unknown zio type");
	return 1;
}

pub int zprintf(zio *s, const char *fmt, ...)
{
	va_list args;

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
	char buf[len + 1];
	va_start(args, fmt);
	len = vsnprintf(buf, len + 1, fmt, args);
	va_end(args);

	/*
	 * Write the string
	 */
	int n;
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
			char c;
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
