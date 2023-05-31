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
	char *str;
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

pub void readtok(parsebuf_t *b, json_token_t *t)
{
	// Skip spaces
	while (isspace(buf_peek(b))) {
		buf_get(b);
	}

	int c = buf_peek(b);
	switch(c)
	{
		case EOF:
			return;
		case '"':
			readstr(b, t);
			return;
		case ':':
		case ',':
		case '{':
		case '}':
		case '[':
		case ']':
			t->type = buf_get(b);
			return;
	}

	if(c == '-' || isdigit(c)) {
		readnum(b, t);
		return;
	}

	if(isalpha(c)) {
		readkw0(b, t);
		return;
	}

	t->type = T_ERR;
	t->str = "Unexpected character";
}

/*
 * number = [ minus ] int [ frac ] [ exp ]
 */
void readnum(parsebuf_t *b, json_token_t *t)
{
	bool minus = false;
	double num = 0.0;

	// minus
	if(buf_peek(b) == '-') {
		minus = true;
		buf_get(b);
	}

	// int
	if(!isdigit(buf_peek(b))) {
		t->type = T_ERR;
		t->str = "Digit expected";
		return;
	}
	while(isdigit(buf_peek(b))) {
		num *= 10;
		num += buf_get(b) - '0';
	}

	// frac
	if(buf_peek(b) == '.') {
		buf_get(b);
		if(!isdigit(buf_peek(b))) {
			t->type = T_ERR;
			t->str = "Digit expected";
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

		if(!isdigit(buf_peek(b))) {
			t->type = T_ERR;
			t->str = "Digit expected";
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

void readkw0(parsebuf_t *b, json_token_t *t)
{
	char kw[8] = {};
	int i = 0;

	while(isalpha(buf_peek(b)) && i < 7) {
		kw[i] = buf_get(b);
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
		t->type = T_ERR;
		t->str = "Unknown keyword";
	}
}

void readstr(parsebuf_t *b, json_token_t *t)
{
	char g = buf_get(b);
	if (g != '"') {
		t->type = T_ERR;
		t->str = "'\"' expected";
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
			if(ch == EOF) {
				t->type = T_ERR;
				t->str = "Unexpected end of file";
				return;
			}
		}

		/*
		 * Append it to the string
		 */
		if(!str_addc(s, ch)) {
			str_free(s);
			t->str = "Out of memory";
			t->type = T_ERR;
			return;
		}
	}

	g = buf_get(b);
	if (g != '"') {
		t->str = "'\"' expected";
		t->type = T_ERR;
		return;
	}

	/*
	 * Create a plain copy of the string
	 */
	char *copy = malloc(str_len(s) + 1);
	if(!copy) {
		free(s);
		t->str = "Out of memory";
		t->type = T_ERR;
		return;
	}

	strcpy(copy, str_raw(s));
	str_free(s);
	t->type = T_STR;
	t->str = copy;
}