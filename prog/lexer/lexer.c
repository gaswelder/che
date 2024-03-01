#import formats/json
#import parsebuf
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
	parsebuf.parsebuf_t *buf;
} lexer_t;

int main() {
	lexer_t *lexer = calloc(1, sizeof(lexer_t));
	if (!lexer) {
		fprintf(stderr, "failed to get lexer: %s\n", strerror(errno));
		return 1;
	}
	lexer->buf = parsebuf.from_stdin();
	if (!lexer->buf) {
		fprintf(stderr, "failed to get parsebuf: %s\n", strerror(errno));
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
	parsebuf.buf_free(l->buf);
	free(l);
}

typedef {
	char *name;
	char *content;
	char *pos;
} tok_t;


/*
 * Creates a new token object.
 * Undo: tok_free(t);
 */
tok_t *tok_make(char *name, char *content, char *pos) {
	tok_t *t = calloc(1, sizeof(tok_t));
	if (!t) {
		return NULL;
	}
	t->name = name;
	t->content = content;
	t->pos = pos;
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
	json.json_node *tok = json.json_newobj();
	json.json_put(tok, "type", json.json_newstr(t->name));
	json.json_put(tok, "content", json.json_newstr(t->content));
	json.json_put(tok, "pos", json.json_newstr(t->pos));
	return json.format(tok);
}

tok_t *lexer_read(lexer_t *l) {
	parsebuf.parsebuf_t *b = l->buf;

	parsebuf.spaces(b);
	if (!parsebuf.buf_more(b)) {
		return NULL;
	}

	int peek = parsebuf.buf_peek(b);
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
	if (parsebuf.buf_literal_follows(b, "/*")) {
		// puts("mcomm");
		return read_multiline_comment(b);
	}
	if (parsebuf.buf_literal_follows(b, "//")) {
		// puts("comm");
		return read_line_comment(b);
	}

	
	char *pos = parsebuf.buf_pos(b);
	for (size_t i = 0; i < nelem(keywords); i++) {
		const char *keyword = keywords[i];
		if (parsebuf.buf_skip_literal(b, keyword)) {
			// puts("keyword");
			return tok_make(strings.newstr("%s", keyword), NULL, pos);
		}
	}
	for (size_t i = 0; i < nelem(symbols); i++) {
		const char *symbol = symbols[i];
		if (parsebuf.buf_skip_literal(b, symbol)) {
			// puts("symbol");
			return tok_make(strings.newstr("%s", symbol), NULL, pos);
		}
	}
	free(pos);

	if (isalpha(peek) || peek == '_') {
		// puts("ident");
		return read_identifier(b);
	}
	return tok_make("error", strings.newstr("unexpected character: '%c'", peek), parsebuf.buf_pos(b));
}

tok_t *read_macro(parsebuf.parsebuf_t *b) {
	char *s = parsebuf.buf_skip_until(b, "\n");
	return tok_make("macro", s, parsebuf.buf_pos(b));
}

tok_t *read_number(parsebuf.parsebuf_t *b) {
	// If "0x" follows, read a hexademical constant.
	if (parsebuf.buf_skip_literal(b, "0x")) {
		return read_hex_number(b);
	}

	char *pos = parsebuf.buf_pos(b);
	char *num = parsebuf.buf_read_set(b, "0123456789");
	if (parsebuf.buf_peek(b) == '.') {
		parsebuf.buf_get(b);
		char *frac = parsebuf.buf_read_set(b, "0123456789");
		char *modifiers = parsebuf.buf_read_set(b, "ULf");
		// defer free(modifiers);
		// defer free(frac);
		return tok_make("num", strings.newstr("%s.%s%s", num, frac, modifiers), pos);
	}

	char *modifiers = parsebuf.buf_read_set(b, "UL");

	if (parsebuf.buf_more(b) && isalpha(parsebuf.buf_peek(b))) {
		return tok_make("error", strings.newstr("unknown modifier: %c", parsebuf.buf_peek(b)), pos);
	}

	char *result = strings.newstr("%s%s", num, modifiers);
	free(num);
	free(modifiers);
	return tok_make("num", result, pos);
}

tok_t *read_hex_number(parsebuf.parsebuf_t *b) {
	// Skip "0x"
	parsebuf.buf_get(b);
	parsebuf.buf_get(b);

	char *num = parsebuf.buf_read_set(b, "0123456789ABCDEFabcdef");
	char *modifiers = parsebuf.buf_read_set(b, "UL");
	// defer free(num);
	// defer free(modifiers);
	return tok_make("num", strings.newstr("0x%s%s", num, modifiers), parsebuf.buf_pos(b));
}

// // TODO: clip/str: str_new() -> str_new(template, args...)

tok_t *read_string(parsebuf.parsebuf_t *b) {
	char *pos = parsebuf.buf_pos(b);

	// Skip the opening quote
	parsebuf.buf_get(b);
	strbuilder.str *s = strbuilder.str_new();

	while (parsebuf.buf_more(b)) {
		char c = parsebuf.buf_get(b);
		if (c == '"') {
			return tok_make("string", strbuilder.str_unpack(s), pos);
		}
		strbuilder.str_addc(s, c);
		if (c == '\\') {
			strbuilder.str_addc(s, parsebuf.buf_get(b));
		}
	}
	return tok_make("error", strings.newstr("double quote expected"), pos);

	// // Expect the closing quote
	// if (parsebuf.buf_get(b) != '"') {
		
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


	// return tok_make("error", "double quote expected", parsebuf.buf_pos(b));
}

tok_t *read_char(parsebuf.parsebuf_t *b) {
	char *s = calloc(3, 1);
	char *p = s;
	char *pos = parsebuf.buf_pos(b);
	
	parsebuf.buf_get(b);

	if (parsebuf.buf_peek(b) == '\\') {
		*p++ = parsebuf.buf_get(b);
	}
	*p++ = parsebuf.buf_get(b);

	if (parsebuf.buf_peek(b) != '\'') {
		free(s);
		return tok_make("error", strings.newstr("single quote expected"), pos);
	}
	parsebuf.buf_get(b);
	return tok_make("char", s, pos);
}


tok_t *read_multiline_comment(parsebuf.parsebuf_t *b) {
	char *pos = parsebuf.buf_pos(b);
	parsebuf.buf_skip_literal(b, "/*");
	char *comment = parsebuf.buf_skip_until(b, "*/");
	if (!parsebuf.buf_skip_literal(b, "*/")) {
		free(comment);
		return tok_make("error", strings.newstr("'*/' expected"), pos);
	}
	return tok_make("comment", comment, pos);
}

tok_t *read_line_comment(parsebuf.parsebuf_t *b) {
	char *pos = parsebuf.buf_pos(b);
	parsebuf.buf_skip_literal(b, "//");
	return tok_make("comment", parsebuf.buf_skip_until(b, "\n"), pos);
}

tok_t *read_identifier(parsebuf.parsebuf_t *b) {
	strbuilder.str *s = strbuilder.str_new();
	char *pos = parsebuf.buf_pos(b);

	while (parsebuf.buf_more(b)) {
		char c = parsebuf.buf_peek(b);
		if (!isalpha(c) && !isdigit(c) && c != '_') {
			break;
		}
		strbuilder.str_addc(s, parsebuf.buf_get(b));
	}
	return tok_make("word", strbuilder.str_unpack(s), pos);
}
