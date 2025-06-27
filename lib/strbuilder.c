
pub typedef {
	char *data;
	size_t len;
	size_t max;
} str;

pub str *str_new()
{
	str *s = calloc(1, sizeof(str));
	return s;
}

pub void str_free(str *s)
{
	free(s->data);
	free(s);
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

pub bool str_addc(str *s, int ch) {
	if (s->len + 1 >= s->max) {
		if (!grow(s)) {
			return false;
		}
	}
	s->data[s->len++] = ch;
	return true;
}

/*
 * Appends string s to the builder's buffer.
 */
pub bool adds(str *b, const char *s) {
	const char *p = s;
	while (*p != '\0') {
		if (!str_addc(b, *p)) {
			return false;
		}
		p++;
	}
	return true;
}

pub bool addf(str *b, const char *format, ...) {
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
	free(s);
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
