import "parsebuf"

/*
 * Tokenizer
 */

/*
 * A text will be split into tokens. Most of them
 * are simply characters like '{', but there are
 * also "string" and "number" tokens that have additional
 * value.
 */
typedef {
	/*
	 * The "type" will be the same as the character in case
	 * of tokens like '{' or ','. For special cases it will
	 * be one of the constants in the enumeration below.
	 */
	int type;
	char *str;
	double num;
} token_t;

/*
 * These values are above 255 so that they couldn't be
 * confused with byte values in the parsed string.
 */
enum {
	T_ERR = 257,
	T_TRUE,
	T_FALSE,
	T_NULL,
	T_STR,
	T_NUM
};

/*
 * Parsing context.
 */
typedef {
	/*
	 * First error reported during parsing.
	 */
	char err[256];
	parsebuf *buf;
	/*
	 * Look-ahead cache.
	 */
	token_t next;
} parser_t;

/*
 * Returns type of the next token
 */
int peek(parser_t *p)
{
	if(p->next.type == EOF && buf_more(p->buf)) {
		readtok(p, &(p->next));
	}
	return p->next.type;
}

void get(parser_t *p, token_t *t)
{
	/*
	 * Read next value into the cache if necessary.
	 */
	if(p->next.type == EOF) {
		peek(p);
	}

	/*
	 * If t is given, copy the token there.
	 */
	if(t) {
		memcpy(t, &(p->next), sizeof(*t));
	}

	/*
	 * Remove the token from the cache.
	 */
	p->next.type = EOF;
}

void readtok(parser_t *p, token_t *t)
{
	/*
	 * Skip spaces
	 */
	while(isspace(buf_peek(p->buf))) {
		buf_get(p->buf);
	}

	int c = buf_peek(p->buf);
	switch(c)
	{
		case EOF:
			return;
		case '"':
			readstr(p, t);
			return;
		case ':':
		case ',':
		case '{':
		case '}':
		case '[':
		case ']':
			t->type = buf_get(p->buf);
			return;
	}

	if(c == '-' || isdigit(c)) {
		readnum(p, t);
		return;
	}

	if(isalpha(c)) {
		readkw(p, t);
		return;
	}

	t->type = T_ERR;
	error(p, "Unexpected character: %c", c);
}

void readstr(parser_t *p, token_t *t)
{
	assert(expectch(p, '"'));
	str *s = str_new();

	while(buf_more(p->buf) && buf_peek(p->buf) != '"') {
		/*
		 * Get next string character
		 */
		int ch = buf_get(p->buf);
		if(ch == '\\') {
			ch = buf_get(p->buf);
			if(ch == EOF) {
				error(p, "Unexpected end of file");
				t->type = T_ERR;
				return;
			}
		}

		/*
		 * Append it to the string
		 */
		if(!str_addc(s, ch)) {
			str_free(s);
			error(p, "Out of memory");
			t->type = T_ERR;
			return;
		}
	}

	if(!expectch(p, '"')) {
		t->type = T_ERR;
		return;
	}

	/*
	 * Create a plain copy of the string
	 */
	char *copy = malloc(str_len(s) + 1);
	if(!copy) {
		free(s);
		error(p, "Out of memory");
		t->type = T_ERR;
		return;
	}

	strcpy(copy, str_raw(s));
	str_free(s);
	t->type = T_STR;
	t->str = copy;
}

bool expectch(parser_t *p, char c)
{
	char g = buf_get(p->buf);
	if(g != c) {
		error(p, "'%c' expected, got '%c'", c, g);
		return false;
	}
	return true;
}

void readkw(parser_t *p, token_t *t)
{
	char kw[8] = {};
	int i = 0;

	while(isalpha(buf_peek(p->buf)) && i < 7) {
		kw[i] = buf_get(p->buf);
		i++;
	}
	kw[i] = '\0';

	if(strcmp(kw, "true") == 0) {
		t->type = T_TRUE;
	}
	else if(strcmp(kw, "false") == 0) {
		t->type = T_FALSE;
	}
	else if(strcmp(kw, "null") == 0) {
		t->type = T_NULL;
	}
	else {
		error(p, "Unknown keyword: %s", kw);
		t->type = T_ERR;
	}
}

/*
 * number = [ minus ] int [ frac ] [ exp ]
 */
void readnum(parser_t *p, token_t *t)
{
	bool minus = false;
	double num = 0.0;

	// minus
	if(buf_peek(p->buf) == '-') {
		minus = true;
		buf_get(p->buf);
	}

	// int
	if(!isdigit(buf_peek(p->buf))) {
		error(p, "Digit expected");
		t->type = T_ERR;
		return;
	}
	while(isdigit(buf_peek(p->buf))) {
		num *= 10;
		num += buf_get(p->buf) - '0';
	}

	// frac
	if(buf_peek(p->buf) == '.') {
		buf_get(p->buf);
		if(!isdigit(buf_peek(p->buf))) {
			error(p, "Digit expected");
			t->type = T_ERR;
			return;
		}

		double pow = 0.1;
		while(isdigit(buf_peek(p->buf))) {
			int d = buf_get(p->buf) - '0';
			num += d * pow;
			pow /= 10;
		}
	}

	// exp
	int c = buf_peek(p->buf);
	if(c == 'e' || c == 'E') {
		bool eminus = false;
		buf_get(p->buf);
		c = buf_peek(p->buf);

		// [minus/plus] digit...
		if(c == '-') {
			eminus = true;
			buf_get(p->buf);
		}
		else if(c == '+') {
			eminus = false;
			buf_get(p->buf);
		}

		if(!isdigit(buf_peek(p->buf))) {
			error(p, "Digit expected");
			t->type = T_ERR;
			return;
		}

		int e = 0;
		while(isdigit(buf_peek(p->buf))) {
			e *= 10;
			e += buf_get(p->buf) - '0';
		}

		double mul = 0;
		if (eminus) mul = 0.1;
		else mul = 10;
		while(e-- > 0) {
			num *= mul;
		}
	}

	if(minus) num *= -1;
	t->type = T_NUM;
	t->num = num;
}
