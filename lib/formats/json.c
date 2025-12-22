#import arr
#import strbuilder
#import strings
#import tokenizer

pub typedef {
	char err[256]; // First error reported during parsing.
	tokenizer.t *buf;
} parser_t;

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

const char *typename(int valtype) {
	switch(valtype) {
		case TUND: { return "und"; }
		case TERR: { return "err"; }
		case TARR: { return "arr"; }
		case TOBJ: { return "obj"; }
		case TSTR: { return "str"; }
		case TNUM: { return "num"; }
		case TBOOL: { return "bool"; }
		case TNULL: { return "null"; }
	}
	return "?";
}

// An opaque object representing any value representable in JSON.
pub typedef {
	int type;
	union {
		char *str;
		double num;
		arr.arr_t *arr;
		arr.arr_t *obj;
		bool boolval;
	} val;
} val_t;

typedef {
	char *key;
	val_t *val;
} kv_t;

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

pub const char *json_typename(val_t *n)
{
	return typename(n->type);
}



// Returns the type of the given node.
pub int type(val_t *obj) {
	if (!obj) return TNULL;
	return obj->type;
}

// Returns the number of keys in an object node.
pub size_t nkeys(val_t *n) {
	if (!n || n->type != TOBJ) {
		return 0;
	}
	return arr.arr_len(n->val.obj);
}

// Returns the i-th key in the object node n.
pub const char *key(val_t *n, size_t i) {
	if (!n || n->type != TOBJ) {
		return NULL;
	}
	arr.arr_t *a = n->val.obj;
	size_t len = arr.arr_len(a);
	if (i >= len) return NULL;
	kv_t *kv = arr.arr_get(a, i);
	return kv->key;
}

// Returns the value stored under the i-th key in the object node n.
pub val_t *json_val(val_t *n, size_t i) {
	if (!n || n->type != TOBJ) {
		return NULL;
	}
	arr.arr_t *a = n->val.obj;
	size_t len = arr.arr_len(a);
	if (i >= len) {
		return NULL;
	}
	kv_t *kv = arr.arr_get(a, i);
	return kv->val;
}

// Returns the value stored under the given key of the given object.
// Returns NULL if the node is not an object or there is no such key in it.
pub val_t *get(val_t *n, const char *k) {
	if (!n || n->type != TOBJ) {
		return NULL;
	}
	arr.arr_t *a = n->val.obj;
	size_t len = arr.arr_len(a);
	for (size_t i = 0; i < len; i++) {
		kv_t *kv = arr.arr_get(a, i);
		if (strcmp(kv->key, k) == 0) {
			return kv->val;
		}
	}
	return NULL;
}

// Returns the value stored at the given index of the given array.
// Returns NULL if the node is not an array or the given index is out of bounds.
pub val_t *json_at(val_t *n, int index) {
	if (!n || n->type != TARR) {
		return NULL;
	}
	arr.arr_t *a = n->val.arr;
	if (index < 0 || (size_t) index >= arr.arr_len(a)) {
		return NULL;
	}
	return arr.arr_get(a, index);
}

// Returns the length of the array object.
// Returns 0 if the object is not an array.
pub int json_len( val_t *obj ) {
	if (!obj || obj->type != TARR) {
		return 0;
	}
	return arr.arr_len(obj->val.arr);
}

// Frees the value.
pub void json_free(val_t *node) {
	if (!node) return;

	if (node->type == TOBJ) {
		arr.arr_t *a = node->val.obj;
		size_t n = arr.arr_len(a);
		for(size_t i = 0; i < n; i++) {
			kv_t *kv = arr.arr_get(a, i);
			free(kv->key);
			json_free(kv->val);
			free(kv);
		}
		arr.arr_free(a);
	} else if (node->type == TARR) {
		arr.arr_t *a = node->val.obj;
		size_t n = arr.arr_len(a);
		for(size_t i = 0; i < n; i++) {
			json_free(arr.arr_get(a, i));
		}
		arr.arr_free(a);
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
		size_t n = nkeys(obj);
		for(size_t i = 0; i < n; i++) {
			char *k = strings.newstr("%s", key(obj, i));
			val_t *v = json_copy(json_val(obj, i));
			if(!k || !v || !json_put(copy, k, v)) {
				free(k);
				json_free(v);
				json_free(copy);
				return NULL;
			}
		}
	}
	else if( obj->type == TARR )
	{
		size_t n = json_len(obj);
		for(size_t i = 0; i < n; i++) {
			val_t *v = json_copy(json_at(obj, i));
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

// Returns a pointer to the string under the given key.
// If the key doesn't exist or the value is not a string, returns NULL.
pub const char *getstr(val_t *obj, const char *k) {
	val_t *v = get(obj, k);
	if (!v || v->type != TSTR) {
		return NULL;
	}
	return v->val.str;
}

/**
 * Returns the node's value assuming it's a number.
 */
pub double json_getdbl(val_t *obj, const char *k) {
	val_t *v = get(obj, k);
	if (!v || v->type != TNUM) {
		return 0.0;
	}
	return v->val.num;
}

// Returns the value of the boolean node n.
// If n is not a boolean node, returns false.
pub bool json_bool(val_t *n) {
	if (n != NULL && n->type == TBOOL) {
		return n->val.boolval;
	}
	return false;
}

// Returns the string from the given string value.
pub const char *json_str(val_t *n) {
	if (n != NULL && n->type == TSTR) {
		return n->val.str;
	}
	return "";
}

pub double json_dbl(val_t *n) {
	if (n != NULL && n->type == TNUM) {
		return n->val.num;
	}
	return 0.0;
}

pub bool json_put( val_t *n, const char *k, val_t *val ) {
	if (n == NULL || n->type != TOBJ) {
		return false;
	}

	/*
	 * Find if the given key already exists.
	 */
	kv_t *kv = NULL;
	arr.arr_t *a = n->val.obj;
	size_t len = arr.arr_len(a);
	for(size_t i = 0; i < len; i++) {
		kv = arr.arr_get(a, i);
		if(strcmp(kv->key, k) == 0) {
			break;
		}
		kv = NULL;
	}

	/*
	 * If such key already exists, delete its value.
	 * If not, create the key.
	 */
	if(kv) {
		json_free(kv->val);
	}
	else {
		kv = calloc!(1, sizeof(*kv));
		kv->key = strings.newstr("%s", k);
		arr.arr_push(a, kv);
	}

	kv->val = val;
	return true;
}

/*
 * Pushes given value to the end of the given array node.
 * Returns false if the given node is not an array and in case of error.
 */
pub bool json_push( val_t *n, val_t *val ) {
	if (n == NULL || n->type != TARR) {
		return false;
	}
	return arr.arr_push(n->val.arr, val);
}

//
// Readers
//

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
	a->val.obj = arr.arr_new();
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
	o->val.obj = arr.arr_new();
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
	size_t len = nkeys(n);
	bool ok = strbuilder.adds(s, "{");
	for (size_t i = 0; i < len; i++) {
		ok = ok
			&& strbuilder.adds(s, "\"")
			&& strbuilder.adds(s, key(n, i))
			&& strbuilder.adds(s, "\":")
			&& writenode(json_val(n, i), s);
		if (i + 1 < len) {
			ok = ok && strbuilder.adds(s, ",");
		}
	}
	ok = ok && strbuilder.adds(s, "}");
	return ok;
}

bool writearr(val_t *n, strbuilder.str *s) {
	size_t len = json_len(n);
	bool ok = strbuilder.adds(s, "[");
	for (size_t i = 0; i < len; i++) {
		ok = ok && writenode(json_at(n, i), s);
		if (i + 1 < len) {
			ok = ok && strbuilder.adds(s, ",");
		}
	}
	ok = ok && strbuilder.adds(s, "]");
	return ok;
}

bool writestr(val_t *n, strbuilder.str *s) {
	const char *content = json_str(n);
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
	double v = json_dbl(n);
	if (v == round(v)) {
		sprintf(buf, "%.0f", json_dbl(n));
	} else {
		sprintf(buf, "%f", json_dbl(n));
	}
	return strbuilder.adds(s, buf);
}

bool writebool(val_t *n, strbuilder.str *s) {
	if (json_bool(n)) {
		return strbuilder.adds(s, "true");
	} else {
		return strbuilder.adds(s, "false");
	}
}

//
// Ac-hoc write utils
//

// Writes a string representation of val into str.
pub int sprintval(val_t *val, char *str) {
	switch (val->type) {
		case TSTR: { return sprintf(str, "%s", val->val.str); }
		case TOBJ: { return sprintf(str, "%s", "(object)"); }
		case TNULL: { return sprintf(str, "%s", "null"); }
		case TARR: { return sprintf(str, "%s", "(array)"); }
		case TNUM: { return sprintf(str, "%g", val->val.num); }
		case TBOOL: {
			if (val->val.boolval) {
				return sprintf(str, "%s", "true");
			} else {
				return sprintf(str, "%s", "false");
			}
		}
		default: { return sprintf(str, "(unimplemented type %d)", val->type); }
	}
}

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
