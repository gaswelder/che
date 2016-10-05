
struct __str {
	char *data;
	size_t len;
	size_t max;
};

typedef struct __str str;

str *str_new()
{
	str *s = calloc(1, sizeof(*s));
	return s;
}

void str_free(str *s)
{
	free(s->data);
	free(s);
}

bool str_addc(str *s, int ch)
{
	if(s->len + 1 >= s->max) {
		if(!grow(s)) {
			return false;
		}
	}

	s->data[s->len++] = ch;
	return true;
}

bool str_adds(str *s, const char *a)
{
	const char *c = a;
	while(*c) {
		if(!str_addc(s, *c)) {
			return false;
		}
		c++;
	}
	return true;
}

char *str_raw(str *s)
{
	return s->data;
}

size_t str_len(str *s)
{
	return s->len;
}

static bool grow(str *s)
{
	if(s->max > SIZE_MAX/2) {
		return false;
	}

	size_t new;
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
