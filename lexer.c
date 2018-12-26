import "lib/parsebuf"
import "lib/string"
import "lib/strutil"

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
	"sizeof"
	"union",
	"const",
	"while",
	"defer",
	"case",
	"enum",
	"else",
	"pub",
	"for",
	"if",
};

struct lexer_struct {
	char *source;
	parsebuf *buf;
};
typedef struct lexer_struct lexer_t;

bool streq(char *a, char *b) {
	return strcmp(a, b) == 0;
}

int main() {
    char *source = read_stdin();
    defer free(source);

    lexer_t *lexer = lexer_make(source);
    defer lexer_free(lexer);

    while (true) {
        tok_t *tok = lexer_read(lexer);
		if (!tok) {
			break;
		}

		if (streq(tok->name, "error")) {
			fprintf(stderr, "%s at %s\n", tok->content, tok->pos);
			tok_free(tok);
			return 1;
		}
		char *json = tok_json(tok);
		fprintf(stdout, "%s\n", json);
		free(json);

		tok_free(tok);
    }

    return 0;
}

char *read_stdin() {
	str *s = str_new();
	while (!feof(stdin)) {
		char c = fgetc(stdin);
		if (c == EOF) break;
		str_addc(s, c);
	}
	return str_unpack(s);
}



lexer_t *lexer_make(char *source) {
	lexer_t *l = calloc(1, sizeof(lexer_t));
	l->source = source;
	l->buf = buf_new(source);
	return l;
}

void lexer_free(lexer_t *l) {
	buf_free(l->buf);
	free(l);
}


struct s_token {
	char *name;
	char *content;
	char *pos;
};
typedef struct s_token tok_t;


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
	return newstr("* %s - %s", t->name, t->content);
}

tok_t *lexer_read(lexer_t *l) {
	parsebuf *b = l->buf;

	buf_skip_set(b, " \r\n\t");
	if (!buf_more(b)) {
		return NULL;
	}

	char peek = buf_peek(b);

	if (peek == '#') {
		puts("macro");
		return read_macro(b);
	}
	if (isdigit(peek)) {
		puts("number");
		return read_number(b);
	}
	if (peek == '\"') {
		puts("string");
		return read_string(b);
	}
	if (peek == '\'') {
		puts("char");
		return read_char(b);
	}
	if (buf_skip_literal(b, "/*")) {
		puts("mcomm");
		return read_multiline_comment(b);
	}
	if (buf_skip_literal(b, "//")) {
		puts("comm");
		return read_line_comment(b);
	}
	for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
		const char *keyword = keywords[i];
		if (buf_skip_literal(b, keyword)) {
			puts("keyword");
			return tok_make(newstr("%s", keyword), NULL, buf_pos(b));
		}
	}
	for (size_t i = 0; i < sizeof(symbols) / sizeof(symbols[0]); i++) {
		const char *symbol = symbols[i];
		if (buf_skip_literal(b, symbol)) {
			puts("symbol");
			return tok_make(newstr("%s", symbol), NULL, buf_pos(b));
		}
	}
	if (isalpha(peek) || peek == '_') {
		puts("ident");
		return read_identifier(b);
	}
	return tok_make("error", newstr("unexpected character: '%c'", peek), buf_pos(b));
	
}

tok_t *read_macro(parsebuf *b) {
	char *s = buf_skip_until(b, "\n");
	return tok_make("macro", s, buf_pos(b));
}

tok_t *read_number(parsebuf *b) {
	// If "0x" follows, read a hexademical constant.
	if (buf_skip_literal(b, "0x")) {
		return read_hex_number(b);
	}

	char *num = buf_read_set(b, "0123456789");
	if (buf_peek(b) == '.') {
		buf_get(b);
		char *frac = buf_read_set(b, "0123456789");
		char *modifiers = buf_read_set(b, "ULf");
		// defer free(modifiers);
		// defer free(frac);
		return tok_make("num", newstr("%s.%s%s", num, frac, modifiers), buf_pos(b));
	}

	char *modifiers = buf_read_set(b, "UL");

	if (buf_more(b) && isalpha(buf_peek(b))) {
		return tok_make("error", newstr("unknown modifier: %c", buf_peek(b)), buf_pos(b));
	}

	char *result = newstr("%s%s", num, modifiers);
	free(num);
	free(modifiers);
	return tok_make("num", result, buf_pos(b));
}

tok_t *read_hex_number(parsebuf *b) {
	// Skip "0x"
	buf_get(b);
	buf_get(b);

	char *num = buf_read_set(b, "0123456789ABCDEFabcdef");
	char *modifiers = buf_read_set(b, "UL");
	// defer free(num);
	// defer free(modifiers);
	return tok_make("num", newstr("0x%s%s", num, modifiers), buf_pos(b));
}

// // TODO: clip/str: str_new() -> str_new(template, args...)

tok_t *read_string(parsebuf *b) {
	// Skip the opening quote
	buf_get(b);

	str *s = str_new();
	str_addc(s, '"');

	while (buf_more(b)) {
		char c = buf_get(b);
		str_addc(s, c);
		if (c == '"') {
			return tok_make("string", str_unpack(s), buf_pos(b));
		}

		if (c == '\\') {
			str_addc(s, buf_get(b));
		}
	}
	return tok_make("error", newstr("double quote expected"), buf_pos(b));

	// // Expect the closing quote
	// if (buf_get(b) != '"') {
		
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
		// 	return token::make("string", $str, $pos);
		// }


	// return tok_make("error", "double quote expected", buf_pos(b));
}

tok_t *read_char(parsebuf *b) {
	char *s = calloc(5, 1);
	char *p = s;
	
	// skip the quote
	buf_get(b);
	*p++ = '\'';

	if (buf_peek(b) == '\\') {
		*p++ = buf_get(b);
	}
	*p++ = buf_get(b);

	if (buf_peek(b) != '\'') {
		free(s);
		return tok_make("error", newstr("single quote expected"), buf_pos(b));
	}
	*p++ = buf_get(b);
	return tok_make("char", s, buf_pos(b));
}


tok_t *read_multiline_comment(parsebuf *b) {
	char *comment = buf_skip_until(b, "*/");
	if (!buf_skip_literal(b, "*/")) {
		free(comment);
		return tok_make("error", newstr("'*/' expected"), buf_pos(b));
	}
	return tok_make("comment", comment, buf_pos(b));
}

tok_t *read_line_comment(parsebuf *b) {
	return tok_make("comment", buf_skip_until(b, "\n"), buf_pos(b));
}

tok_t *read_identifier(parsebuf *b) {
	str *s = str_new();

	while (buf_more(b)) {
		char c = buf_peek(b);
		if (!isalpha(c) && !isdigit(c) && c != '_') {
			break;
		}
		str_addc(s, buf_get(b));
	}
	return tok_make("word", str_unpack(s), buf_pos(b));
}
