#import strbuilder
#import strings
#import tokenizer
#import writer

// JSON data is represented as a tree where each node is an object of type `val_t`.
// A node can be of one of the following types:
pub enum {
	TUND = 0,
	TERR = 1,
	TARR = 2, // array
	TOBJ = 3, // object
	TSTR = 4, // string
	TNUM = 5, // number
	TBOOL = 6, // true or false
	TNULL = 7 // null value
}

// An opaque object representing any value representable in JSON.
pub typedef {
	int type;
	union {
		char *str;
		double num;
		bool boolval;
	} val;
	size_t size; // Number of entries, for arrays and objects.
	size_t cap; // Max. number of entries.
	char **keys; // Entry keys, for objects.
	val_t **vals; // Values, for objects and arrays.
} val_t;

pub const char *json_err(val_t *n)
{
	if(n == NULL) {
		return "No memory";
	}
	if(n->type == TERR) {
		return n->val.str;
	}
	return NULL;
}

val_t *newnode(int valtype) {
	val_t *n = calloc!(1, sizeof(val_t));
	n->type = valtype;
	return n;
}

// Frees the value.
pub void json_free(val_t *node) {
	if (!node) return;

	if (node->type == TOBJ) {
		for (size_t i = 0; i < node->size; i++) {
			free(node->keys[i]);
			json_free(node->vals[i]);
		}
		free(node->keys);
		free(node->vals);
	} else if (node->type == TARR) {
		for (size_t i = 0; i < node->size; i++) {
			json_free(node->vals[i]);
		}
		free(node->vals);
	}
	else if( node->type == TSTR || node->type == TERR ) {
		free(node->val.str);
	}

	free(node);
}

// Creates a deep copy of the value.
pub val_t *json_copy(val_t *obj) {
	if (!obj) {
		return NULL;
	}
	val_t *copy = calloc!(1, sizeof(val_t));
	memcpy(copy, obj, sizeof(val_t));

	if( obj->type == TOBJ )
	{
		size_t n = obj->size;
		for(size_t i = 0; i < n; i++) {
			char *k = strings.newstr("%s", key(obj, i));
			val_t *v = json_copy(val(obj, i));
			if (!k || !v || !json_put(copy, k, v)) {
				free(k);
				json_free(v);
				json_free(copy);
				return NULL;
			}
		}
	}
	else if( obj->type == TARR )
	{
		size_t n = obj->size;
		for (size_t i = 0; i < n; i++) {
			val_t *v = json_copy(obj->vals[i]);
			if(!v || !json_push(copy, v)) {
				json_free(v);
				json_free(copy);
				return NULL;
			}
		}
	}
	else if( obj->type == TSTR )
	{
		copy->val.str = strings.newstr("%s", obj->val.str);
		if(!copy->val.str) {
			free(copy);
			return NULL;
		}
	}

	return copy;
}

pub bool json_put( val_t *n, const char *k, val_t *val ) {
	if (n == NULL || n->type != TOBJ) {
		return false;
	}

	// Find if the given key already exists.
	size_t pos = 0;
	for (pos = 0; pos < n->size; pos++) {
		if (strcmp(n->keys[pos], k) == 0) {
			break;
		}
	}

	// If key already exists, delete the old value and put the new one.
	if (pos < n->size) {
		json_free(n->vals[pos]);
		n->vals[pos] = val;
	} else {
		grow_if_needed(n);
		n->vals[n->size] = val;
		n->keys[n->size] = strings.newstr("%s", k);
		n->size++;
	}
	return true;
}

// Pushes value val to the array value n.
bool json_push(val_t *n, val_t *val) {
	if (n == NULL || n->type != TARR) {
		return false;
	}
	grow_if_needed(n);
	n->vals[n->size] = val;
	n->size++;
	return true;
}

void grow_if_needed(val_t *n) {
	if (n->size < n->cap) {
		return;
	}
	n->cap *= 2;
	if (n->cap == 0) n->cap = 16;
	n->vals = realloc(n->vals, n->cap * sizeof(val_t *));
	if (!n->vals) {
		panic("realloc failed");
	}
	if (n->type == TOBJ) {
		n->keys = realloc(n->keys, n->cap * sizeof(char *));
		if (!n->keys) {
			panic("realloc failed");
		}
	}
}

//
// Value getters
//

// Returns the type of the value v.
pub int type(val_t *v) {
	if (!v) return TNULL;
	return v->type;
}

// Returns the number of entries in the array or object value v.
pub size_t len(val_t *v) {
	return v->size;
}

// Returns the i-th key in object value v.
pub const char *key(val_t *v, size_t i) {
	if (!v || v->type != TOBJ || i >= v->size) {
		return NULL;
	}
	return v->keys[i];
}

// Returns the i-th value in object or array value v.
pub val_t *val(val_t *v, size_t i) {
	if (!v || v->type != TOBJ || i >= v->size) {
		return NULL;
	}
	return v->vals[i];
}

// Returns the value stored under key k in object v.
// Returns NULL if v is not an object or there is no value at key k.
pub val_t *get(val_t *v, const char *k) {
	if (!v || v->type != TOBJ) {
		return NULL;
	}
	for (size_t i = 0; i < v->size; i++) {
		if (strcmp(v->keys[i], k) == 0) {
			return v->vals[i];
		}
	}
	return NULL;
}

// Returns a pointer to the string of the string value v.
// Returns null if v is not a string value.
pub const char *strval(val_t *v) {
	if (!v || v->type != TSTR) {
		return NULL;
	}
	return v->val.str;
}

//
// Parser
//

pub typedef {
	char err[256]; // First error reported during parsing.
	tokenizer.t *buf;
} parser_t;

// Parses JSON string s and returns a pointer to the root val_t object.
//
// In the case of error, a special error node is returned.
// The user should check the returned node using the `json_error` function.
// The returned node has to be freed using the `json_free` in both cases.
// The `json_free` function must be called only on root nodes.
pub val_t *parse(const char *s) {
	parser_t p = {};
	p.buf = tokenizer.from_str(s);

	val_t *result = read_node(&p);
	if (!result) {
		tokenizer.free(p.buf);
		return NULL;
	}

	// Expect end of file at this point.
	if (tok_more(&p)) {
		// char buf[100];
		// tokenizer.tail(p.buf, buf, 100);
		// panic("trailing input: %s", buf);
		return NULL;
	}

	tokenizer.free(p.buf);
	return result;
}

// Reads one node and returns it.
// Returns null in case of error.
val_t *read_node(parser_t *p) {
	if (haserr(p)) return NULL;
	if (!tok_more(p)) {
		seterror(p, "no more input");
		return NULL;
	}
	if (tok_peek(p) == '[') {
		return read_array(p);
	}
	if (tok_peek(p) == '{') {
		return read_dict(p);
	}
	if (tok_peek(p) == '"') {
		return read_str(p);
	}
	if (isdigit(tok_peek(p)) || tok_peek(p) == '-') {
		return read_num(p);
	}
	return read_kw(p);
}

val_t *read_array(parser_t *p) {
	if (!expect(p, '[')) {
		return NULL;
	}
	val_t *a = newnode(TARR);
	if (eat(p, ']')) {
		return a;
	}
	while (tok_more(p)) {
		val_t *x = read_node(p);
		if (!x) {
			return NULL;
		}
		json_push(a, x);
		if (!eat(p, ',')) {
			break;
		}
	}
	if (!expect(p, ']')) {
		return NULL;
	}
	return a;
}

val_t *read_dict(parser_t *p) {
	if (!expect(p, '{')) {
		return NULL;
	}
	val_t *o = newnode(TOBJ);
	if (eat(p, '}')) {
		return o;
	}
	while (tok_more(p)) {
		char *key = readstr(p);
		if (!key) {
			return NULL;
		}
		if (!expect(p, ':')) {
			return NULL;
		}
		val_t *val = read_node(p);
		if (!val) {
			return NULL;
		}
		json_put(o, key, val);
		if (!eat(p, ',')) {
			break;
		}
	}
	if (!expect(p, '}')) {
		return NULL;
	}
	return o;
}

val_t *read_str(parser_t *p) {
	char *s = readstr(p);
	if (!s) {
		return NULL;
	}
	val_t *n = newnode(TSTR);
	n->val.str = s;
	return n;
}

val_t *read_num(parser_t *p) {
	char buf[100] = {};
	if (!tokenizer.num(p->buf, buf, sizeof(buf))) {
		seterror(p, "failed to read number");
		return NULL;
	}
	double n = 0;
	if (sscanf(buf, "%lf", &n) < 1) {
		seterror(p, "failed to parse number: %s", buf);
		return NULL;
	}
	val_t *v = newnode(TNUM);
	v->val.num = n;
	return v;
}

val_t *read_kw(parser_t *p) {
	tokenizer.t *b = p->buf;
	if (tokenizer.skip_literal(b, "true")) {
		val_t *n = newnode(TBOOL);
		n->val.boolval = true;
		return n;
	}
	if (tokenizer.skip_literal(b, "false")) {
		val_t *n = newnode(TBOOL);
		n->val.boolval = false;
		return n;
	}
	if (tokenizer.skip_literal(b, "null")) {
		return newnode(TNULL);
	}
	seterror(p, "unexpected character: %c", tok_peek(p));
	return NULL;
}

char *readstr(parser_t *p) {
	size_t cap = 64;
	size_t len = 0;
	char *s = calloc!(cap, 1);
	if (!expect(p, '"')) {
		free(s);
	}
	tokenizer.t *b = p->buf;
	while (tokenizer.more(p->buf) && tokenizer.peek(p->buf) != '"') {
		int c = tokenizer.get(b);
		if (c == '\\') {
			c = tokenizer.get(b);
			if (c == EOF) {
				seterror(p, "Unexpected end of input");
				free(s);
			}
		}
		if (len + 1 == cap) {
			cap *= 2;
			s = realloc(s, cap);
			if (!s) {
				panic("realloc failed");
			}
		}
		s[len++] = c;
		s[len] = '\0';
	}
	if (!expect(p, '"')) {
		free(s);
	}
	return s;
}

// Returns true if there are more characters to read.
bool tok_more(parser_t *p) {
	tokenizer.spaces(p->buf);
	return tokenizer.more(p->buf);
}

// Returns next character without removing it.
int tok_peek(parser_t *p) {
	tokenizer.spaces(p->buf);
	return tokenizer.peek(p->buf);
}

// Reads character c and returns true.
// Returns false and does nothing if the next character is not c.
bool eat(parser_t *p, int c) {
	if (tok_peek(p) == c) {
		tokenizer.get(p->buf);
		return true;
	}
	return false;
}

bool expect(parser_t *p, int c) {
	if (!eat(p, c)) {
		seterror(p, "expected '%c', got '%c' (%d)", c, tok_peek(p), tok_peek(p));
		return false;
	}
	return true;
}

bool haserr(parser_t *p) {
	return p->err[0] != '\0';
}

void seterror(parser_t *p, const char *fmt, ...) {
	va_list args = {};
	va_start(args, fmt);
	vsnprintf(p->err, sizeof(p->err), fmt, args);
	va_end(args);
}


//
// Writers
//

pub void formatwr(writer.t *w, val_t *v) {
	if (!v) panic("v is null");
	char *s = format(v);
	if (!s) panic("format failed");
	size_t n = strlen(s);
	uint8_t *buf = (uint8_t *) s;
	while (n > 0) {
		int r = writer.write(w, buf, n);
		if (r <= 0) {
			panic("write failed");
		}
		n -= (size_t) r;
	}
}

// Returns a string with the value n formatted as json.
// The caller must free the string after use.
pub char *format(val_t *n) {
	strbuilder.str *s = strbuilder.new();
	if (!writenode(n, s)) {
		strbuilder.str_free(s);
		return NULL;
	}
	return strbuilder.str_unpack(s);
}

bool writenode(val_t *n, strbuilder.str *s) {
	switch(type(n)) {
		case TOBJ: { return writeobj(n, s); }
		case TARR: { return writearr(n, s); }
		case TSTR: { return writestr(n, s); }
		case TNUM: { return writenum(n, s); }
		case TBOOL: { return writebool(n, s); }
		case TNULL: { return strbuilder.adds(s, "null"); }
	}
	panic("unhandled json node type: %d", type(n));
}

bool writeobj(val_t *n, strbuilder.str *s) {
	size_t len = n->size;
	bool ok = strbuilder.adds(s, "{");
	for (size_t i = 0; i < len; i++) {
		ok = ok
			&& strbuilder.adds(s, "\"")
			&& strbuilder.adds(s, key(n, i))
			&& strbuilder.adds(s, "\":")
			&& writenode(val(n, i), s);
		if (i + 1 < len) {
			ok = ok && strbuilder.adds(s, ",");
		}
	}
	ok = ok && strbuilder.adds(s, "}");
	return ok;
}

bool writearr(val_t *n, strbuilder.str *s) {
	size_t len = n->size;
	bool ok = strbuilder.adds(s, "[");
	for (size_t i = 0; i < len; i++) {
		ok = ok && writenode(n->vals[i], s);
		if (i + 1 < len) {
			ok = ok && strbuilder.adds(s, ",");
		}
	}
	ok = ok && strbuilder.adds(s, "]");
	return ok;
}

bool writestr(val_t *n, strbuilder.str *s) {
	const char *content = n->val.str;
	size_t len = strlen(content);
	bool ok = strbuilder.adds(s, "\"");
	for (size_t i = 0; i < len; i++) {
		const char c = content[i];
		if (c == '\n') {
			ok = ok
				&& strbuilder.str_addc(s, '\\')
				&& strbuilder.str_addc(s, 'n');
			continue;
		}
		if (c == '\t') {
			ok = ok
				&& strbuilder.str_addc(s, '\\')
				&& strbuilder.str_addc(s, 't');
			continue;
		}
		if (c == '\\' || c == '\"' || c == '/') {
			ok = ok && strbuilder.str_addc(s, '\\');
		}
		ok = ok && strbuilder.str_addc(s, c);
	}
	ok = ok && strbuilder.str_addc(s, '"');
	return ok;
}

bool writenum(val_t *n, strbuilder.str *s) {
	char buf[100] = {0};
	double v = n->val.num;
	if (v == round(v)) {
		sprintf(buf, "%.0f", n->val.num);
	} else {
		sprintf(buf, "%f", n->val.num);
	}
	return strbuilder.adds(s, buf);
}

bool writebool(val_t *n, strbuilder.str *s) {
	if (n->val.boolval) {
		return strbuilder.adds(s, "true");
	} else {
		return strbuilder.adds(s, "false");
	}
}

//
// Ac-hoc write utils
//

// ad-hoc function
// write an escaped quoted string s to f.
pub void write_string(FILE *f, const char *s) {
	fputc('"', f);
	while (*s != '\0') {
		switch (*s) {
			case '\n': { fputc('\\', f); fputc('n', f); }
			case '\t': { fputc('\\', f); fputc('t', f); }
			case '\\': { fputc('\\', f); fputc('\\', f); }
			case '\"': { fputc('\\', f); fputc('"', f); }
			default: { fputc(*s, f); }
		}
		s++;
	}
	fputc('"', f);
}
