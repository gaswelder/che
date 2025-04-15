#import parsebuf

pub typedef {
	// One of the "T_" constance below.
	int type;
	// Token payload, for tokens of type T_ERR, T_STR and T_NUM.
	char *str;
} json_token_t;

/*
 * These values are above 255 so that they couldn't be
 * confused with byte values in the parsed string.
 */
pub enum {
	T_ERR = 257,
	T_TRUE,
	T_FALSE,
	T_NULL,
	T_STR,
	T_NUM
};

pub typedef {
    parsebuf.parsebuf_t *buf;
    json_token_t next;
	size_t strlen;
	size_t strcap;
} json_tokenizer_t;

/**
 * Returns a new lexer for the given string.
 */
pub json_tokenizer_t *new(const char *s) {
    json_tokenizer_t *t = calloc(1, sizeof(json_tokenizer_t));
    if (!t) {
        return t;
    }
    t->buf = parsebuf.from_str(s);
    if (!t->buf) {
        free(t);
        return NULL;
    }
	t->strcap = 4096;
	t->next.str = calloc(1, 4096);
	if (!t->next.str) {
		parsebuf.buf_free(t->buf);
		free(t);
		return NULL;
	}
    return t;
}

/**
 * Frees the lexer's memory.
 */
pub void free(json_tokenizer_t *t) {
    parsebuf.buf_free(t->buf);
    OS.free(t);
}

/**
 * Reads next token into the local buffer.
 * Returns false if there was an error.
 */
pub bool read(json_tokenizer_t *t) {
	parsebuf.spaces(t->buf);
    int c = parsebuf.peek(t->buf);
    if (c == EOF) {
        t->next.type = EOF;
    }
    else if (c == '"') {
        readstr(t);
    }
    else if (c == ':' || c == ',' || c == '{' || c == '}' || c == '[' || c == ']') {
        t->next.type = parsebuf.buf_get(t->buf);
    }
    else if (c == '-' || isdigit(c)) {
		readnum(t);
	}
    else if (isalpha(c)) {
		readkw(t);
	}
    else {
		seterror(t, "Unexpected character");
    }
	if (t->next.type == T_ERR) {
        return false;
    }
    return true;
}

/*
 * Returns the type of the current token.
 */
pub int currtype(json_tokenizer_t *l) {
	return l->next.type;
}

/*
 * Returns current token's string value.
 */
pub const char *currstr(json_tokenizer_t *l) {
	if (l->next.type != T_STR && l->next.type != T_ERR && l->next.type != T_NUM) {
		return NULL;
	}
	return l->next.str;
}

/*
 * number = [ minus ] int [ frac ] [ exp ]
 */
void readnum(json_tokenizer_t *l)
{
	parsebuf.parsebuf_t *b = l->buf;
	json_token_t *t = &l->next;

	char buf[100] = {};
	if (!parsebuf.num(b, buf, 100)) {
		seterror(l, "failed to parse a number");
		return;
	}

	resetstr(l);
	for (char *p = buf; *p; p++) {
		if (!addchar(l, *p)) {
			seterror(l, "out of memory");
			return;
		}
	}
	t->type = T_NUM;
}

void readkw(json_tokenizer_t *l)
{
	parsebuf.parsebuf_t *b = l->buf;
	json_token_t *t = &l->next;
	char kw[8] = {};
	int i = 0;

	while (isalpha(parsebuf.peek(b)) && i < 7) {
		kw[i] = parsebuf.buf_get(b);
		i++;
	}
	kw[i] = '\0';

	switch str (kw) {
		case "true": { t->type = T_TRUE; }
		case "false": { t->type = T_FALSE; }
		case "null": { t->type = T_NULL; }
		default: { seterror(l, "Unknown keyword"); }
	}
}

void readstr(json_tokenizer_t *l)
{
	parsebuf.parsebuf_t *b = l->buf;
	json_token_t *t = &l->next;

	char g = parsebuf.buf_get(b);
	if (g != '"') {
		seterror(l, "'\"' expected");
		return;
	}
	resetstr(l);

	while (parsebuf.buf_more(b) && parsebuf.peek(b) != '"') {
		// Get next string character
		int ch = parsebuf.buf_get(b);
		if (ch == '\\') {
			ch = parsebuf.buf_get(b);
			if (ch == EOF) {
				seterror(l, "Unexpected end of input");
				return;
			}
		}

		// Append it to the string
		if (!addchar(l, ch)) {
			seterror(l, "Out of memory");
			return;
		}
	}

	g = parsebuf.buf_get(b);
	if (g != '"') {
		seterror(l, "'\"' expected");
		return;
	}
	t->type = T_STR;
}

void seterror(json_tokenizer_t *l, const char *s) {
	l->next.type = T_ERR;
	putstring(l, "%s", s);
}

bool addchar(json_tokenizer_t *l, int c) {
	// if no space, resize
	if (l->strlen >= l->strcap) {
		l->strcap *= 2;
		l->next.str = realloc(l->next.str, l->strcap);
		if (!l->next.str) {
			return false;
		}
	}
	l->next.str[l->strlen++] = c;
	l->next.str[l->strlen] = '\0';
	return true;
}

void resetstr(json_tokenizer_t *l) {
	l->strlen = 0;
}

/*
 * Puts a formatted string into the lexer's local buffer.
 * Returns false in case of error.
 */
bool putstring(json_tokenizer_t *l, const char *format, ...) {
	va_list args = {};

	// get the total length
	va_start(args, format);
	int len = vsnprintf(NULL, 0, format, args);
	va_end(args);
	if (len < 0) {
		return false;
	}

	// if buffer is smaller, resize the buffer
	if (l->strlen < (size_t)len + 1) {
		size_t newsize = findsize(len + 1);
		l->next.str = realloc(l->next.str, newsize);
		l->strlen = newsize;
		if (l->next.str == NULL) {
			return false;
		}
	}

	// put the string
	va_start(args, format);
	vsnprintf(l->next.str, l->strlen, format, args);
	va_end(args);
	return true;
}

size_t findsize(int len) {
	size_t r = 1;
	size_t n = (size_t) len;
	while (r < n) {
		r *= 2;
	}
	return r;
}
