#import parsebuf

/*
 * A text will be split into tokens. Most of them are simply characters
 * like '{', but there are also "string" and "number" tokens that have
 * additional value.
 */
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

// json_tokenizer_t *tmp = new_json_tokenizer(s);
	// while (true) {
	// 	lexer_read_next(tmp);
	// 	json_token_t *t = lexer_curr(tmp);
	// 	switch (t->type) {
	// 		case T_ERR: printf("err(%s)\n", t->str); break;
	// 		case T_TRUE: printf("T_TRUE\n"); break;
	// 		case T_FALSE: printf("T_FALSE\n"); break;
	// 		case T_NULL: printf("T_NULL\n"); break;
	// 		case T_STR: printf("T_STR(%s)\n", t->str); break;
	// 		case T_NUM: printf("T_NUM(%f)\n", t->num); break;
	// 		case '[': printf("[\n"); break;
	// 		case ']': printf("]\n"); break;
	// 		case '{': printf("{\n"); break;
	// 		case '}': printf("}\n"); break;
	// 		case ',': printf(",\n"); break;
	// 		case ':': printf(":\n"); break;
	// 		case '"': printf("\"\n"); break;
    //     	default: printf("unknown type\n"); return NULL;
	// 	}
	// }

// const char *toktypestring(int t) {
//     switch (t) {
//         case T_ERR: return "T_ERR";
// 	    case T_TRUE: return "T_TRUE";
// 	    case T_FALSE: return "T_FALSE";
// 	    case T_NULL: return "T_NULL";
// 	    case T_STR: return "T_STR";
// 	    case T_NUM: return "T_NUM";
//         case '[': return "[";
//         case ']': return "]";
//         case '{': return "{";
//         case '}': return "}";
//         case ',': return ",";
//         case ':': return ":";
//         case '"': return "\"";
//         default: return "(unknown type)";
//     }
// }

pub typedef {
    parsebuf_t *buf;
    json_token_t next;
	size_t strlen;
	size_t strcap;
} json_tokenizer_t;

pub json_tokenizer_t *new_json_tokenizer(const char *s) {
    json_tokenizer_t *t = calloc(1, sizeof(json_tokenizer_t));
    if (!t) {
        return t;
    }
    t->buf = buf_new(s);
    if (!t->buf) {
        free(t);
        return NULL;
    }
	t->strcap = 4096;
	t->next.str = calloc(1, 4096);
	if (!t->next.str) {
		buf_free(t->buf);
		free(t);
		return NULL;
	}
    return t;
}

pub void free_json_tokenizer(json_tokenizer_t *t) {
    buf_free(t->buf);
    free(t);
}

/**
 * Reads next token into the local buffer.
 * Returns false if there was an error.
 */
pub bool lexer_read_next(json_tokenizer_t *t) {
	// Skip spaces
	while (isspace(buf_peek(t->buf))) {
		buf_get(t->buf);
	}
    int c = buf_peek(t->buf);
    if (c == EOF) {
        t->next.type = EOF;
    }
    else if (c == '"') {
        readstr(t);
    }
    else if (c == ':' || c == ',' || c == '{' || c == '}' || c == '[' || c == ']') {
        t->next.type = buf_get(t->buf);
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
pub int lexer_currtype(json_tokenizer_t *l) {
	return l->next.type;
}

/*
 * Returns current token's string value.
 */
pub const char *lexer_currstr(json_tokenizer_t *l) {
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
	parsebuf_t *b = l->buf;
	json_token_t *t = &l->next;
	resetstr(l);

	// Optional minus
	if (buf_peek(b) == '-') {
		addchar(l, buf_get(b));
	}

	// Integer part
	if (!isdigit(buf_peek(b))) {
		seterror(l, "Digit expected");
		return;
	}
	while (isdigit(buf_peek(b))) {
		addchar(l, buf_get(b));
	}

	// Optional fractional part
	if (buf_peek(b) == '.') {
		addchar(l, buf_get(b));
		if (!isdigit(buf_peek(b))) {
			seterror(l, "Digit expected");
			return;
		}
		while (isdigit(buf_peek(b))) {
			addchar(l, buf_get(b));
		}
	}

	// Optional exponent
	int c = buf_peek(b);
	if (c == 'e' || c == 'E') {
		addchar(l, buf_get(b));
		
		// Optional - or +
		c = buf_peek(b);
		if (c == '-') {
			addchar(l, buf_get(b));
		}
		else if (c == '+') {
			addchar(l, buf_get(b));
		}

		// Sequence of exponent digits
		if (!isdigit(buf_peek(b))) {
			seterror(l, "Digit expected");
			return;
		}
		while (isdigit(buf_peek(b))) {
			addchar(l, buf_get(b));
		}
	}
	t->type = T_NUM;
}

void readkw(json_tokenizer_t *l)
{
	parsebuf_t *b = l->buf;
	json_token_t *t = &l->next;
	char kw[8] = {};
	int i = 0;

	while (isalpha(buf_peek(b)) && i < 7) {
		kw[i] = buf_get(b);
		i++;
	}
	kw[i] = '\0';

	if (strcmp(kw, "true") == 0) {
		t->type = T_TRUE;
	}
	else if (strcmp(kw, "false") == 0) {
		t->type = T_FALSE;
	}
	else if (strcmp(kw, "null") == 0) {
		t->type = T_NULL;
	}
	else {
		seterror(l, "Unknown keyword");
	}
}

void readstr(json_tokenizer_t *l)
{
	parsebuf_t *b = l->buf;
	json_token_t *t = &l->next;

	char g = buf_get(b);
	if (g != '"') {
		seterror(l, "'\"' expected");
		return;
	}
	resetstr(l);

	while (buf_more(b) && buf_peek(b) != '"') {
		// Get next string character
		int ch = buf_get(b);
		if (ch == '\\') {
			ch = buf_get(b);
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

	g = buf_get(b);
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