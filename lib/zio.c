import "memio"
import "net"

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


zio *zopen(const char *type, const char *name, const char *mode)
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

void zclose(zio *s)
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

int zread(zio *s, char *buf, int size)
{
	switch(s->type) {
		case S_FILE:
			size_t r = fread(buf, 1, (size_t) size, s->h);
			return (int) r;
		case S_MEM:
			return memread(buf, 1, (size_t) size, s->h);
		case S_TCP:
			return net_read(s->h, buf, (size_t) size);
		default:
			puts("Unknown zio type");
	}
	return -1;
}

int zwrite(zio *s, const char *buf, int len)
{
	switch(s->type) {
		case S_FILE:
			return (int) fwrite(buf, 1, (size_t) len, s->h);
		case S_MEM:
			return memwrite(buf, 1, (size_t) len, s->h);
		case S_TCP:
			return net_write(s->h, buf, (size_t) len);
		default:
			puts("Unknown zio type");
	}
	return -1;
}

int zrewind(zio *s)
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
