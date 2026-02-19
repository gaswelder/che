
pub typedef {
	char *data;
	size_t len;
	size_t max;
	bool err;
} str;

// Creates a new instance of builder.
pub str *new() {
	str *s = calloc!(1, sizeof(str));
	return s;
}

// Frees the builder and its buffer.
pub void free(str *s) {
	OS.free(s->data);
	OS.free(s);
}

/**
 * Clears the contents of the builder so it can be used again.
 */
pub void clear(str *s) {
	if (s->len == 0) {
		return;
	}
	memset(s->data, 0, s->len);
	s->len = 0;
}

// Appends a single character to the buider's buffer.
pub bool addc(str *s, int ch) {
	if (s->err) return false;
	if (s->len + 1 >= s->max) {
		if (!grow(s)) {
			s->err = true;
			return false;
		}
	}
	s->data[s->len++] = ch;
	return true;
}

// Appends string s to the builder's buffer.
pub bool adds(str *b, const char *s) {
	if (b->err) return false;
	const char *p = s;
	while (*p != '\0') {
		if (!addc(b, *p)) {
			return false;
		}
		p++;
	}
	return true;
}

pub bool addf(str *b, const char *format, ...) {
	if (b->err) return false;
	char buf[1000] = {};
	va_list l = {0};
	va_start(l, format);
	vsnprintf(buf, sizeof(buf), format, l);
	va_end(l);
	return adds(b, buf);
}

pub char *str_raw(str *s)
{
	return s->data;
}

pub size_t str_len(str *s)
{
	return s->len;
}

/*
 * Frees the string container, but return the resulting string.
 * The string returned will have to be deallocated by the caller.
 */
pub char *str_unpack(str *s) {
	char *c = s->data;
	OS.free(s);
	return c;
}

bool grow(str *s)
{
	if(s->max > SIZE_MAX/2) {
		return false;
	}

	size_t new = 0;
	if(s->max == 0) {
		new = 16;
	}
	else {
		new = s->max * 2;
	}
	char *tmp = realloc(s->data, new);
	if(!tmp) {
		return false;
	}
	s->data = tmp;
	s->max = new;
	for(size_t i = s->len; i < s->max; i++) {
		s->data[i] = '\0';
	}
	return true;
}
