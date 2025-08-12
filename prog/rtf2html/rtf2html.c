#import tokenizer

// Current styling, indexes are the style name constants.
enum { ALIGN, FONTSIZE, FONT }
int style[3] = {};

// Align values
enum { CENTER = 1 }
char *aligns[] = { "left", "center" };

bool doc_started = false;
bool par_started = false;
bool style_started = false;

bool has_styles() {
	return style[0] != 0 || style[1] != 0 || style[2] != 0;
}

void addchar(int c) {
	if (!par_started && isspace(c)) {
		return;
	}
	if (!doc_started) {
		printf("<!DOCTYPE html>\n<html>\n<head></head>\n<body>\n");
		doc_started = true;
	}
	if (!par_started) {
		printf("<p>");
		par_started = true;
	}
	if (!style_started && has_styles()) {
		printf("<span style=\"");
		if (style[ALIGN] != 0) {
			printf("display:block;text-align:%s;", aligns[style[ALIGN]]);
		}
		if (style[FONTSIZE] != 0) {
			printf("font-size:%gpt;", (float)style[FONTSIZE]/2);
		}
		if (style[FONT] != 0) {
			printf("font-family:%d;", style[FONT]);
		}
		printf("\">");
		style_started = true;
	}
	putchar(c);
}

void lwrite(const char *s) {
	while (*s != '\0') { addchar(*s++); }
}

void setstyle(int key, val) {
	if (style_started) {
		printf("</span>");
		style_started = false;
	}
	style[key] = val;
}

void resetstyles() {
	setstyle(ALIGN, 0);
	setstyle(FONTSIZE, 0);
	setstyle(FONT, 0);
}


// Ends current paragraph.
void par() {
	if (style_started) {
		printf("</span>");
		style_started = false;
	}
	if (par_started) {
		printf("</p>\n\n");
		par_started = false;
	}
}


int main(int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, "arguments: rtf-file\n");
		return 1;
	}
	FILE *f = fopen(argv[1], "rb");
	if (!f) {
		fprintf(stderr, "failed to open '%s': %s\n", argv[1], strerror(errno));
		return 1;
	}
	tokenizer.t *t = tokenizer.file(f);
	body(t);
	tokenizer.free(t);
	fclose(f);
	return 0;
}

void body(tokenizer.t *t) {
	expect(t, '{');
	while (tokenizer.more(t)) {
		if (tokenizer.peek(t) == '}') {
			break;
		}
		if (tokenizer.peek(t) == '\\') {
			control(t);
			continue;
		}
		addchar(tokenizer.get(t));
	}
	expect(t, '}');
	if (doc_started) {
		printf("</body>\n</html>\n");
	}
}

void control(tokenizer.t *t) {
	char buf[20] = {};
	read_control(t, buf);
	switch str (buf) {
		case "par": { par(); } 
		case "pard": { resetstyles(); }
		case "qc": { setstyle(ALIGN, CENTER); }
		case "ldblquote": { lwrite("&ldquo;"); }
		case "rdblquote": { lwrite("&rdquo; "); }
		case "fs20": { setstyle(FONTSIZE, 20); }
		case "fs30": { setstyle(FONTSIZE, 30); }
		case "deflang1033": { deflang(t); }
		case "f0": { setstyle(FONT, 0); }
		case "ansi": {} // Latin-1 charset
		case "ansicpg1252": {} // Latin-1 charset (again)
		case "deff0": {} // default font = 0
		case "deff0": {} // default font number
		case "fi360": {} // first-line indent = 360
		case "ri-1800": {} // right indent
		case "rtf1": {} // RTF v1 marker
		case "uc1": {} // bytes per character
		case "viewkind4": {} // 1 = "page layout view", 4 = "normal view", etc
		default: { panic("unknown control: %s", buf); }
	}
}

void deflang(tokenizer.t *t) {
	expect(t, '{');
	expect_control(t, "fonttbl");
	fonttbl(t);
	expect(t, '}');
}

void fonttbl(tokenizer.t *t) {
	char font[100] = {};
	expect(t, '{');
	expect_control(t, "f0");
	expect_control(t, "fnil");
	expect_control(t, "fcharset0");
	tokenizer.read_until(t, ';', font, sizeof(font));
	expect(t, ';');
	expect(t, '}');
}

// -------------------

void read_control(tokenizer.t *t, char *buf) {
	expect(t, '\\');
	char *p = buf;
	while (tokenizer.more(t) && OS.isalnum(tokenizer.peek(t))) {
		*p++ = tokenizer.get(t);
	}
	if (tokenizer.peek(t) == '-') {
		*p++ = tokenizer.get(t);
		while (isdigit(tokenizer.peek(t))) *p++ = tokenizer.get(t);
	}
	while (isspace(tokenizer.peek(t))) tokenizer.get(t);
}

void expect(tokenizer.t *t, int c) {
	int r = tokenizer.get(t);
	if (r != c) {
		panic("expected %c, got %c", c, r);
	}
}

void expect_control(tokenizer.t *t, char *control) {
	char buf[20] = {};
	read_control(t, buf);
	if (strcmp(buf, control) != 0) {
		panic("expected %s, got %s", control, buf);
	}
}
