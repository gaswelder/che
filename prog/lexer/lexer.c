#import formats/json
#import scanner
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
	scanner.t *buf;
} lexer_t;

int main() {
	lexer_t *lexer = calloc!(1, sizeof(lexer_t));
	lexer->buf = scanner.file(stdin);
	if (!lexer->buf) {
		panic("failed to get scanner");
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
	scanner.free(l->buf);
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
	scanner.t *b = l->buf;

	scanner.spaces(b);
	if (!scanner.more(b)) {
		return NULL;
	}

	int peek = scanner.peek(b);
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
	if (scanner.literal_follows(b, "/*")) {
		// puts("mcomm");
		return read_multiline_comment(b);
	}
	if (scanner.literal_follows(b, "//")) {
		// puts("comm");
		return read_line_comment(b);
	}

	const char *pos = scanner.posstr(b);
	for (size_t i = 0; i < nelem(keywords); i++) {
		const char *keyword = keywords[i];
		if (scanner.skip_literal(b, keyword)) {
			// puts("keyword");
			return tok_make(strings.newstr("%s", keyword), NULL, pos);
		}
	}
	for (size_t i = 0; i < nelem(symbols); i++) {
		const char *symbol = symbols[i];
		if (scanner.skip_literal(b, symbol)) {
			// puts("symbol");
			return tok_make(strings.newstr("%s", symbol), NULL, pos);
		}
	}

	if (isalpha(peek) || peek == '_') {
		// puts("ident");
		return read_identifier(b);
	}
	return tok_make("error", strings.newstr("unexpected character: '%c'", peek), scanner.posstr(b));
}

tok_t *read_macro(scanner.t *b) {
	char *s = scanner.buf_skip_until(b, "\n");
	return tok_make("macro", s, scanner.posstr(b));
}

tok_t *read_number(scanner.t *b) {
	// If "0x" follows, read a hexademical constant.
	if (scanner.skip_literal(b, "0x")) {
		return read_hex_number(b);
	}

	const char *pos = scanner.posstr(b);
	char *num = scanner.buf_read_set(b, "0123456789");
	if (scanner.peek(b) == '.') {
		scanner.get(b);
		char *frac = scanner.buf_read_set(b, "0123456789");
		char *modifiers = scanner.buf_read_set(b, "ULf");
		// defer free(modifiers);
		// defer free(frac);
		return tok_make("num", strings.newstr("%s.%s%s", num, frac, modifiers), pos);
	}

	char *modifiers = scanner.buf_read_set(b, "UL");

	if (scanner.more(b) && isalpha(scanner.peek(b))) {
		return tok_make("error", strings.newstr("unknown modifier: %c", scanner.peek(b)), pos);
	}

	char *result = strings.newstr("%s%s", num, modifiers);
	free(num);
	free(modifiers);
	return tok_make("num", result, pos);
}

tok_t *read_hex_number(scanner.t *b) {
	// Skip "0x"
	scanner.get(b);
	scanner.get(b);

	char *num = scanner.buf_read_set(b, "0123456789ABCDEFabcdef");
	char *modifiers = scanner.buf_read_set(b, "UL");
	// defer free(num);
	// defer free(modifiers);
	return tok_make("num", strings.newstr("0x%s%s", num, modifiers), scanner.posstr(b));
}

// // TODO: clip/str: new() -> new(template, args...)

tok_t *read_string(scanner.t *b) {
	const char *pos = scanner.posstr(b);

	// Skip the opening quote
	scanner.get(b);
	strbuilder.str *s = strbuilder.new();

	while (scanner.more(b)) {
		char c = scanner.get(b);
		if (c == '"') {
			return tok_make("string", strbuilder.str_unpack(s), pos);
		}
		strbuilder.addc(s, c);
		if (c == '\\') {
			strbuilder.addc(s, scanner.get(b));
		}
	}
	return tok_make("error", strings.newstr("double quote expected"), pos);

	// // Expect the closing quote
	// if (scanner.get(b) != '"') {
		
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


	// return tok_make("error", "double quote expected", scanner.posstr(b));
}

tok_t *read_char(scanner.t *b) {
	char *s = calloc!(3, 1);
	char *p = s;
	const char *pos = scanner.posstr(b);
	
	scanner.get(b);

	if (scanner.peek(b) == '\\') {
		*p++ = scanner.get(b);
	}
	*p++ = scanner.get(b);

	if (scanner.peek(b) != '\'') {
		free(s);
		return tok_make("error", strings.newstr("single quote expected"), pos);
	}
	scanner.get(b);
	return tok_make("char", s, pos);
}


tok_t *read_multiline_comment(scanner.t *b) {
	const char *pos = scanner.posstr(b);
	scanner.skip_literal(b, "/*");
	char *comment = scanner.buf_skip_until(b, "*/");
	if (!scanner.skip_literal(b, "*/")) {
		free(comment);
		return tok_make("error", strings.newstr("'*/' expected"), pos);
	}
	return tok_make("comment", comment, pos);
}

tok_t *read_line_comment(scanner.t *b) {
	const char *pos = scanner.posstr(b);
	scanner.skip_literal(b, "//");
	return tok_make("comment", scanner.buf_skip_until(b, "\n"), pos);
}

tok_t *read_identifier(scanner.t *b) {
	strbuilder.str *s = strbuilder.new();
	const char *pos = scanner.posstr(b);

	while (scanner.more(b)) {
		char c = scanner.peek(b);
		if (!isalpha(c) && !isdigit(c) && c != '_') {
			break;
		}
		strbuilder.addc(s, scanner.get(b));
	}
	return tok_make("word", strbuilder.str_unpack(s), pos);
}
