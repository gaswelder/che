#import parsebuf
#import string

/*
 * Tokenizer
 */


/*
 * A text will be split into tokens. Most of them
 * are simply characters like '{', but there are
 * also "string" and "number" tokens that have additional
 * value.
 */
pub typedef {
	/*
	 * The "type" will be the same as the character in case
	 * of tokens like '{' or ','. For special cases it will
	 * be one of the constants in the enumeration below.
	 */
	int type;
	const char *str;
	double num;
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

pub json_token_t *lexer_curr(json_tokenizer_t *t) {
    return &t->next;
}

/*
 * number = [ minus ] int [ frac ] [ exp ]
 */
void readnum(json_tokenizer_t *l)
{
	parsebuf_t *b = l->buf;
	json_token_t *t = &l->next;
	bool minus = false;
	double num = 0.0;

	// minus
	if(buf_peek(b) == '-') {
		minus = true;
		buf_get(b);
	}

	// int
	if (!isdigit(buf_peek(b))) {
		seterror(l, "Digit expected");
		return;
	}
	while (isdigit(buf_peek(b))) {
		num *= 10;
		num += buf_get(b) - '0';
	}

	// frac
	if(buf_peek(b) == '.') {
		buf_get(b);
		if (!isdigit(buf_peek(b))) {
			seterror(l, "Digit expected");
			return;
		}

		double pow = 0.1;
		while(isdigit(buf_peek(b))) {
			int d = buf_get(b) - '0';
			num += d * pow;
			pow /= 10;
		}
	}

	// exp
	int c = buf_peek(b);
	if(c == 'e' || c == 'E') {
		bool eminus = false;
		buf_get(b);
		c = buf_peek(b);

		// [minus/plus] digit...
		if(c == '-') {
			eminus = true;
			buf_get(b);
		}
		else if(c == '+') {
			eminus = false;
			buf_get(b);
		}

		if (!isdigit(buf_peek(b))) {
			seterror(l, "Digit expected");
			return;
		}

		int e = 0;
		while(isdigit(buf_peek(b))) {
			e *= 10;
			e += buf_get(b) - '0';
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
	
	str *s = str_new();

	while(buf_more(b) && buf_peek(b) != '"') {
		/*
		 * Get next string character
		 */
		int ch = buf_get(b);
		if(ch == '\\') {
			ch = buf_get(b);
			if (ch == EOF) {
				seterror(l, "Unexpected end of input");
				return;
			}
		}

		/*
		 * Append it to the string
		 */
		if(!str_addc(s, ch)) {
			str_free(s);
			seterror(l, "Out of memory");
			return;
		}
	}

	g = buf_get(b);
	if (g != '"') {
		seterror(l, "'\"' expected");
		return;
	}

	/*
	 * Create a plain copy of the string
	 */
	char *copy = malloc(str_len(s) + 1);
	if (!copy) {
		free(s);
		seterror(l, "Out of memory");
		return;
	}

	strcpy(copy, str_raw(s));
	str_free(s);
	t->type = T_STR;
	t->str = copy;
}

void seterror(json_tokenizer_t *l, const char *s) {
	l->next.type = T_ERR;
	l->next.str = s;
}
