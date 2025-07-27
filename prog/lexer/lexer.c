#import formats/json
#import tokenizer
#import strbuilder
#import strings

/*
 * Sorted by length, longest first.
 */
const char *symbols[] = {
	"<<=", ">>=", "...",
	"++", "--", "->", "<<", ">>", "<=", ">=", "&&", "||", "+=", "-=", "*=",
	"/=", "%=", "&=", "^=", "|=", "==", "!=", "!", "~", "&", "^", "*", "/",
	"%", "=", "|", ":", ",", "<", ">", "+", "-", "{", "}", ";", "[", "]",
	"(", ")", ".", "?"
};

const char *keywords[] = {
	"typedef",
	"struct",
	"import",
	"return",
	"switch",
	"sizeof",
	"union",
	"const",
	"while",
	"case",
	"enum",
	"else",
	"pub",
	"for",
	"if"
};

typedef {
	tokenizer.t *buf;
} lexer_t;

int main() {
	lexer_t *lexer = calloc!(1, sizeof(lexer_t));
	lexer->buf = tokenizer.stdin();
	if (!lexer->buf) {
		fprintf(stderr, "failed to get tokenizer: %s\n", strerror(errno));
		return 1;
	}

    while (true) {
        tok_t *tok = lexer_read(lexer);
		if (!tok) {
			break;
		}

		if (strcmp(tok->name, "error") == 0) {
			fprintf(stderr, "%s at %s\n", tok->content, tok->pos);
			tok_free(tok);
			return 1;
		}
		char *json = tok_json(tok);
		fprintf(stdout, "%s\n", json);
		// free(json);

		tok_free(tok);
    }
	lexer_free(lexer);
    return 0;
}

void lexer_free(lexer_t *l) {
	tokenizer.free(l->buf);
	free(l);
}

typedef {
	char *name;
	char *content;
	char *pos;
} tok_t;


// Creates a new token object.
tok_t *tok_make(char *name, char *content, const char *pos) {
	tok_t *t = calloc!(1, sizeof(tok_t));
	t->name = name;
	t->content = content;
	t->pos = strings.newstr("%s", pos);
	return t;
}

/*
 * Deallocates a token object.
 */
void tok_free(tok_t *t) {
	free(t->content);
	free(t->pos);
	free(t);
}

char *tok_json(tok_t *t) {
	json.val_t *tok = json.json_newobj();
	json.json_put(tok, "type", json.json_newstr(t->name));
	json.json_put(tok, "content", json.json_newstr(t->content));
	json.json_put(tok, "pos", json.json_newstr(t->pos));
	return json.format(tok);
}

tok_t *lexer_read(lexer_t *l) {
	tokenizer.t *b = l->buf;

	tokenizer.spaces(b);
	if (!tokenizer.more(b)) {
		return NULL;
	}

	int peek = tokenizer.peek(b);
	if (peek == '#') {
		// puts("macro");
		return read_macro(b);
	}
	if (isdigit(peek)) {
		// puts("number");
		return read_number(b);
	}
	if (peek == '\"') {
		// puts("string");
		return read_string(b);
	}
	if (peek == '\'') {
		// puts("char");
		return read_char(b);
	}
	if (tokenizer.literal_follows(b, "/*")) {
		// puts("mcomm");
		return read_multiline_comment(b);
	}
	if (tokenizer.literal_follows(b, "//")) {
		// puts("comm");
		return read_line_comment(b);
	}

	const char *pos = tokenizer.posstr(b);
	for (size_t i = 0; i < nelem(keywords); i++) {
		const char *keyword = keywords[i];
		if (tokenizer.skip_literal(b, keyword)) {
			// puts("keyword");
			return tok_make(strings.newstr("%s", keyword), NULL, pos);
		}
	}
	for (size_t i = 0; i < nelem(symbols); i++) {
		const char *symbol = symbols[i];
		if (tokenizer.skip_literal(b, symbol)) {
			// puts("symbol");
			return tok_make(strings.newstr("%s", symbol), NULL, pos);
		}
	}

	if (isalpha(peek) || peek == '_') {
		// puts("ident");
		return read_identifier(b);
	}
	return tok_make("error", strings.newstr("unexpected character: '%c'", peek), tokenizer.posstr(b));
}

tok_t *read_macro(tokenizer.t *b) {
	char *s = tokenizer.buf_skip_until(b, "\n");
	return tok_make("macro", s, tokenizer.posstr(b));
}

tok_t *read_number(tokenizer.t *b) {
	// If "0x" follows, read a hexademical constant.
	if (tokenizer.skip_literal(b, "0x")) {
		return read_hex_number(b);
	}

	const char *pos = tokenizer.posstr(b);
	char *num = tokenizer.buf_read_set(b, "0123456789");
	if (tokenizer.peek(b) == '.') {
		tokenizer.get(b);
		char *frac = tokenizer.buf_read_set(b, "0123456789");
		char *modifiers = tokenizer.buf_read_set(b, "ULf");
		// defer free(modifiers);
		// defer free(frac);
		return tok_make("num", strings.newstr("%s.%s%s", num, frac, modifiers), pos);
	}

	char *modifiers = tokenizer.buf_read_set(b, "UL");

	if (tokenizer.more(b) && isalpha(tokenizer.peek(b))) {
		return tok_make("error", strings.newstr("unknown modifier: %c", tokenizer.peek(b)), pos);
	}

	char *result = strings.newstr("%s%s", num, modifiers);
	free(num);
	free(modifiers);
	return tok_make("num", result, pos);
}

tok_t *read_hex_number(tokenizer.t *b) {
	// Skip "0x"
	tokenizer.get(b);
	tokenizer.get(b);

	char *num = tokenizer.buf_read_set(b, "0123456789ABCDEFabcdef");
	char *modifiers = tokenizer.buf_read_set(b, "UL");
	// defer free(num);
	// defer free(modifiers);
	return tok_make("num", strings.newstr("0x%s%s", num, modifiers), tokenizer.posstr(b));
}

// // TODO: clip/str: new() -> new(template, args...)

tok_t *read_string(tokenizer.t *b) {
	const char *pos = tokenizer.posstr(b);

	// Skip the opening quote
	tokenizer.get(b);
	strbuilder.str *s = strbuilder.new();

	while (tokenizer.more(b)) {
		char c = tokenizer.get(b);
		if (c == '"') {
			return tok_make("string", strbuilder.str_unpack(s), pos);
		}
		strbuilder.str_addc(s, c);
		if (c == '\\') {
			strbuilder.str_addc(s, tokenizer.get(b));
		}
	}
	return tok_make("error", strings.newstr("double quote expected"), pos);

	// // Expect the closing quote
	// if (tokenizer.get(b) != '"') {
		
	// }

	// return tok_make("string", )


		 //String
		// if ($s->peek() == """) {
		// 	$str = "";
		// 	// A string literal may be split into parts,
		// 	// so concatenate it.
		// 	while ($s->peek() == """) {
		// 		$str .= $this->read_string();
		// 		$s->read_set(self::spaces);
		// 	}
		// 	return make_token("string", $str, $pos);
		// }


	// return tok_make("error", "double quote expected", tokenizer.posstr(b));
}

tok_t *read_char(tokenizer.t *b) {
	char *s = calloc!(3, 1);
	char *p = s;
	const char *pos = tokenizer.posstr(b);
	
	tokenizer.get(b);

	if (tokenizer.peek(b) == '\\') {
		*p++ = tokenizer.get(b);
	}
	*p++ = tokenizer.get(b);

	if (tokenizer.peek(b) != '\'') {
		free(s);
		return tok_make("error", strings.newstr("single quote expected"), pos);
	}
	tokenizer.get(b);
	return tok_make("char", s, pos);
}


tok_t *read_multiline_comment(tokenizer.t *b) {
	const char *pos = tokenizer.posstr(b);
	tokenizer.skip_literal(b, "/*");
	char *comment = tokenizer.buf_skip_until(b, "*/");
	if (!tokenizer.skip_literal(b, "*/")) {
		free(comment);
		return tok_make("error", strings.newstr("'*/' expected"), pos);
	}
	return tok_make("comment", comment, pos);
}

tok_t *read_line_comment(tokenizer.t *b) {
	const char *pos = tokenizer.posstr(b);
	tokenizer.skip_literal(b, "//");
	return tok_make("comment", tokenizer.buf_skip_until(b, "\n"), pos);
}

tok_t *read_identifier(tokenizer.t *b) {
	strbuilder.str *s = strbuilder.new();
	const char *pos = tokenizer.posstr(b);

	while (tokenizer.more(b)) {
		char c = tokenizer.peek(b);
		if (!isalpha(c) && !isdigit(c) && c != '_') {
			break;
		}
		strbuilder.str_addc(s, tokenizer.get(b));
	}
	return tok_make("word", strbuilder.str_unpack(s), pos);
}
