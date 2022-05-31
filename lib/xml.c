/*
 * A stream-oriented parser that reads files of arbitrary length and
 * defines three operations a program can use: 'enter', 'next', and
 * 'leave'.
 *
 * At any moment there is a "cursor" pointing to some node in the file,
 * and there are three operations that modify that cursor: 'enter',
 * 'next' and 'leave'.
 */

#import cli

#define MAXNAME 16 // max name length
#define MAXSTACK 4 // max nesting level
#define MAXATTRS 64 // max attributes count
#define MAXVALUE 4096 // max attribute value length

// tag types
enum {
	T_NULL,
	T_OPEN, // opening tag
	T_CLOSE, // closing tag
	T_MONO // self-closing tag
};

typedef {
	char name[MAXNAME];
	char value[MAXVALUE];
} __attr;

typedef {
	char name[MAXNAME];
	int type;
	__attr attrs[MAXATTRS];
	int nattrs;
} __tag;

typedef {
	// node we're at right now
	__tag node;

	// parent names stack
	char path[MAXSTACK][MAXNAME];
	int pathlen;

	char error[256];

	FILE *f;
	int line;
	int col;

	int nextchar;

	// lookahead cache
	__tag next_tag;
} xml;

/*
 * Creates a parser that reads from the given file
 */
pub xml *xml_open(const char *path)
{
	FILE *f = fopen(path, "rb");
	if(!f) {
		fatal("fopen(%s) failed", path);
	}

	xml *x = calloc(1, sizeof(xml));
	x->f = f;
	x->line = 1;
	x->col = 1;
	x->nextchar = EOF;

	/*
	 * Discard the xml header
	 */
	discard_spaces(x);
	expect(x, '<');
	expect(x, '?');
	while(peek(x) != EOF && peek(x) != '>') {
		get(x);
	}
	expect(x, '>');

	/*
	 * Go to the first element
	 */
	__tag *n = next_tag(x);
	if(!n) {
		error(x, "Unexpected end of file");
		return x;
	}

	if(n->type != T_OPEN && n->type != T_MONO) {
		error(x, "Element expected");
		return x;
	}

	shift(x);
	return x;
}

/*
 * Closes a parser
 */
pub void xml_close(xml *x)
{
	fclose(x->f);
	free(x);
}

/*
 * Returns name of the current node
 */
pub const char *xml_nodename(xml *x)
{
	if(x->error[0]) {
		return NULL;
	}
	if(x->node.type == T_NULL) {
		return NULL;
	}
	return x->node.name;
}

/*
 * Returns value of given attribute of the current node.
 * Returns NULL if there is no such attribute or current node.
 */
pub const char *xml_attr(xml *x, const char *name)
{
	if(x->error[0]) {
		return NULL;
	}

	__tag *n = &(x->node);
	if(n->type == T_NULL) {
		return NULL;
	}
	int i = 0;
	for(i = 0; i < n->nattrs; i++) {
		if(strcmp(n->attrs[i].name, name) == 0) {
			return n->attrs[i].value;
		}
	}
	return NULL;
}

/*
 * Enter the current node tree
 */
pub bool xml_enter(xml *x)
{
	if(x->error[0]) {
		return false;
	}

	/*
	 * The current node must be of T_OPEN kind.
	 */
	if(x->node.type == T_NULL ||
		x->node.type == T_MONO) {
		return false;
	}

	__tag *next = next_tag(x);
	if(!next) {
		error(x, "Unexpected end of file");
		return false;
	}

	switch(next->type) {
		/*
		 * If open or self-closing tag follows, that is
		 * the first child.
		 */
		case T_OPEN:
		case T_MONO:
			pushparent(x);
			shift(x);
			return true;
		case T_CLOSE:
			if(strcmp(next->name, x->node.name) != 0) {
				error(x, "Unexpected closing tag: %s", x->next_tag.name);
				return false;
			}
			pushparent(x);
			x->node.type = T_NULL;
			return true;
	}
	fatal("xml error 0152");
	return false;
}

/*
 * Go to the next sibling in the current tree
 */
pub void xml_next(xml *x)
{
	if(x->error[0]) {
		return;
	}

	switch(x->node.type) {
		case T_NULL:
			return;
		/*
		 * If current node is a tree, "jump" over it
		 * by entering and leaving.
		 */
		case T_OPEN:
			xml_enter(x);
			xml_leave(x);
			return;

		/*
		 * If current node is a self-closing one,
		 * look at what follows next.
		 */
		case T_MONO:
			__tag *next = next_tag(x);
			if(!next) {
				error(x, "Unexpected end of file");
				return;
			}
			/*
			 * If a non-closing tag follows, shift to it.
			 * Otherwise shift to "null".
			 */
			if(next->type == T_OPEN || next->type == T_MONO) {
				shift(x);
			}
			else {
				x->node.type = T_NULL;
			}
			return;

		default:
			fatal("xml: error 0209");
	}
}

/*
 * Leave current node tree and get to the next sibling
 */
pub void xml_leave(xml *x)
{
	if(x->error[0]) {
		return;
	}

	/*
	 * Skip remaining siblings
	 */
	while(x->node.type != T_NULL) {
		xml_next(x);
	}

	/*
	 * Expect closing tag for current parent
	 */
	__tag *t = next_tag(x);
	const char *pname = x->path[x->pathlen-1];
	if(!t || t->type != T_CLOSE || strcmp(t->name, pname) != 0) {
		error(x, "Missing closing tag for %s", pname);
		return;
	}
	/*
	 * Consume next tag (the closing one checked above)
	 * and remove the parent from the stack
	 */
	x->next_tag.type = T_NULL;
	x->pathlen--;

	/*
	 * Go to the next sibling of the ex-parent.
	 */
	t = next_tag(x);
	if(t && (t->type == T_OPEN || t->type == T_MONO)) {
		shift(x);
	}
	else {
		x->node.type = T_NULL;
	}
}

/*
 * Returns current position in the file
 */
pub long xml_filepos(xml *x)
{
	return ftell(x->f);
}

/*
 * Push current tag name on the stack.
 */
void pushparent(xml *x)
{
	if(x->pathlen >= MAXSTACK) {
		fatal("Reached max stack depth: %d", MAXSTACK);
	}
	strcpy(x->path[x->pathlen], x->node.name);
	x->pathlen++;
}

/*
 * Consume the following tag and set it as current node
 */
void shift(xml *x)
{
	// cur = next
	memcpy(&(x->node), next_tag(x), sizeof(__tag));

	// Consume the tag
	x->next_tag.type = T_NULL;
}

/*
 * Returns a pointer to the next tag from the file.
 */
__tag *next_tag(xml *x)
{
	read_tag(x);
	return &(x->next_tag);
}

/*
 * Reads next tag from the file, if necessary
 */
void read_tag(xml *x)
{
	__tag *t = &(x->next_tag);
	if(t->type != T_NULL) {
		return;
	}

	discard_spaces(x);

	if(feof(x->f)) {
		return;
	}

	expect(x, '<');
	if(peek(x) == '/') {
		t->type = T_CLOSE;
		get(x);
	}

	int len = 0;
	while(isalpha(peek(x))) {
		if(len >= MAXNAME-1) {
			error(x, "name too long");
			return;
		}
		t->name[len++] = get(x);
	}
	t->name[len] = '\0';
	if(!len) {
		error(x, "name expected");
		return;
	}

	if(t->type == T_CLOSE) {
		expect(x, '>');
		return;
	}

	t->nattrs = 0;
	while(1) {
		discard_spaces(x);
		if(!isalpha(peek(x))) {
			break;
		}

		if(t->nattrs >= MAXATTRS) {
			fatal("too many attributes");
		}
		__attr *a = &(t->attrs[t->nattrs++]);

		// name
		int len = 0;
		while(isalpha(peek(x))) {
			if(len >= MAXNAME-1) {
				fatal("attrname too long: %s", a->name);
			}
			a->name[len] = get(x);
			len++;
		}
		a->name[len] = '\0';

		expect(x, '=');

		// val
		expect(x, '"');
		len = 0;
		while(peek(x) != EOF && peek(x) != '"') {
			if(len >= MAXVALUE-1) {
				fatal("attrvalue too long: %s", a->value);
			}
			a->value[len] = get(x);
			len++;
		}
		a->value[len] = '\0';
		expect(x, '"');
	}

	if(peek(x) == '/') {
		t->type = T_MONO;
		get(x);
	}
	else {
		t->type = T_OPEN;
	}

	expect(x, '>');
}

int peek(xml *x)
{
	if(x->nextchar == EOF) {
		x->nextchar = fgetc(x->f);
	}
	return x->nextchar;
}

int get(xml *x)
{
	int c = 0;
	if(x->nextchar != EOF) {
		c = x->nextchar;
		x->nextchar = EOF;
	}
	else {
		c = fgetc(x->f);
	}

	if(c == '\n') {
		x->line++;
		x->col = 1;
	}
	else {
		x->col++;
	}
	return c;
}

void discard_spaces(xml *x)
{
	while(isspace(peek(x))) {
		get(x);
	}
}

void expect(xml *x, int ch)
{
	int next = get(x);
	if(next != ch) {
		error(x, "'%c' expected, got '%c'", ch, next);
	}
}

void error(xml *x, const char *fmt, ...)
{
	va_list l = {};
	va_start(l, fmt);
	vsnprintf(x->error, sizeof(x->error), fmt, l);
	va_end(l);

	va_start(l, fmt);
	vprintf(fmt, l);
	va_end(l);

	printf(" at %d:%d\n", x->line, x->col);
}
