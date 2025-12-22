#import arr
#import strbuilder
#import strings
#import tokenizer

pub typedef {
	int type; // One of the "T_" constants below.
	char *str; // Token payload, for tokens of type T_ERR, T_STR and T_NUM.
} json_token_t;

pub typedef {
	char err[256]; // First error reported during parsing.
	tokenizer.t *buf;
    json_token_t next;
	size_t strlen;
	size_t strcap;
} parser_t;

enum {
	tok_T_ERR = 257,
	tok_T_TRUE,
	tok_T_FALSE,
	tok_T_NULL,
	tok_T_STR,
	tok_T_NUM
}

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

// Creates a copy of the value.
pub val_t *json_copy( val_t *obj )
{
	if( !obj ) return NULL;

	val_t *copy = mcopy( obj, sizeof(val_t) );
	if( !copy ) {
		return NULL;
	}

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

void *mcopy(const void *src, size_t size) {
	void *copy = calloc!(size, 1);
	memcpy(copy, src, size);
	return copy;
}



// Parses JSON string s and returns a pointer to the root val_t object.
//
// In the case of error, a special error node is returned.
// The user should check the returned node using the `json_error` function.
// The returned node has to be freed using the `json_free` in both cases.
// The `json_free` function must be called only on root nodes.
pub val_t *parse(const char *s) {
	parser_t p = {};
    p.buf = tokenizer.from_str(s);
    p.strcap = 4096;
	p.next.str = calloc!(1, 4096);

	if (!tok_read(&p)) {
		error(&p, "%s", p.next.str);
		tokenizer.free(p.buf);
		return NULL;
	}
	if (p.next.type == EOF) {
		tokenizer.free(p.buf);
		return NULL;
	}

	val_t *result = read_node(&p);
	if (!result) {
		tokenizer.free(p.buf);
		return NULL;
	}

	// Expect end of file at this point.
	if (p.next.type != EOF) {
		json_free(result);
		tokenizer.free(p.buf);
		return NULL;
	}

	tokenizer.free(p.buf);
	return result;
}

/*
 * Reads one node and returns it.
 * Returns NULL in case of error.
 */
val_t *read_node(parser_t *p) {
	switch (tok_currtype(p)) {
		case EOF: { return error(p, "Unexpected end of file"); }
		case '[': { return read_array(p); }
		case '{': { return read_dict(p); }
		case tok_T_STR: {
			const char *s = tok_currstr(p);
			val_t *n = newnode(TSTR);
			char *copy = mcopy(s, strlen(s) + 1);
			n->val.str = copy;
			tok_read(p);
			if (tok_currtype(p) == tok_T_ERR) {
				error(p, "%s", s);
			}
			return n;
		}
		case tok_T_NUM: {
			const char *val = tok_currstr(p);
			double n = 0;
			if (sscanf(val, "%lf", &n) < 1) {
				error(p, "failed to parser number: %s", val);
			}
			tok_read(p);
			if (tok_currtype(p) == tok_T_ERR) {
				error(p, "%s", tok_currstr(p));
			}
			val_t *obj = newnode(TNUM);
			obj->val.num = n;
			return obj;
		}
		case tok_T_TRUE: {
			tok_read(p);
			val_t *n = newnode(TBOOL);
			n->val.boolval = true;
			return n;
		}
		case tok_T_FALSE: {
			tok_read(p);
			val_t *n = newnode(TBOOL);
			n->val.boolval = false;
			return n;
		}
		case tok_T_NULL: {
			tok_read(p);
			return newnode(TNULL);
		}
	}
	return NULL;
}

val_t *read_array(parser_t *p) {
	if(!expect(p, '[')) {
		return NULL;
	}
	val_t *a = newnode(TARR);
	a->val.obj = arr.arr_new();
	if (!a->val.obj) {
		panic("alloc failed");
	}
	if (tok_currtype(p) == ']') {
		tok_read(p);
		if (tok_currtype(p) == tok_T_ERR) {
			error(p, "%s", tok_currstr(p));
		}
		return a;
	}

	while (tok_currtype(p) != EOF) {
		val_t *v = read_node(p);
		if (!v) {
			json_free(a);
			return NULL;
		}
		json_push(a, v);
		if(tok_currtype(p) != ',') {
			break;
		}
		tok_read(p);
		if (tok_currtype(p) == tok_T_ERR) {
			error(p, "%s", tok_currstr(p));
		}
	}
	if (tok_currtype(p) != ']') {
		json_free(a);
		return NULL;
	}
	tok_read(p);
	if (tok_currtype(p) == tok_T_ERR) {
		error(p, "%s", tok_currstr(p));
	}
	return a;
}

bool consume(parser_t *p, int toktype) {
	if (tok_currtype(p) != toktype) {
		return false;
	}
	tok_read(p);
	if (tok_currtype(p) == tok_T_ERR) {
		error(p, "%s", tok_currstr(p));
	}
	return true;
}

val_t *read_dict(parser_t *p) {
	if (!expect(p, '{')) {
		return NULL;
	}
	val_t *o = newnode(TOBJ);
	o->val.obj = arr.arr_new();
	if (!o->val.obj) {
		panic("alloc failed");
	}
	if (consume(p, '}')) {
		return o;
	}

	while (tok_currtype(p) != EOF) {
		// Get the field name str.
		if (tok_currtype(p) != tok_T_STR) {
			json_free(o);
			return error(p, "Key expected");
		}
		char *key = strings.newstr("%s", tok_currstr(p));
		if (!key) {
			json_free(o);
			return error(p, "No memory");
		}
		tok_read(p);

		// Get the ":"
		if (!expect(p, ':')) {
			free(key);
			json_free(o);
			return NULL;
		}

		val_t *val = read_node(p);
		if (!val) {
			free(key);
			json_free(o);
			return NULL;
		}

		if (!json_put(o, key, val)) {
			free(key);
			json_free(o);
			json_free(val);
			return NULL;
		}

		if (tok_currtype(p) != ',') {
			break;
		}
		tok_read(p);
		if (tok_currtype(p) == tok_T_ERR) {
			error(p, "%s", tok_currstr(p));
		}
	}
	if (!expect(p, '}')) {
		json_free(o);
		return NULL;
	}
	return o;
}

bool expect(parser_t *p, int toktype)
{
	if (tok_currtype(p) != toktype) {
		error(p, "'%c' expected, got '%c'", toktype, tok_currtype(p));
		return false;
	}
	tok_read(p);
	if (tok_currtype(p) == tok_T_ERR) {
		error(p, "%s", tok_currstr(p));
	}
	return true;
}

/*
 * Puts a formatted error message to the parser's
 * context and returns NULL.
 */
void *error(parser_t *p, const char *fmt, ...) {
	va_list args = {};
	va_start(args, fmt);
	vsnprintf(p->err, sizeof(p->err), fmt, args);
	va_end(args);
	return NULL;
}

/**
 * Reads next token into the local buffer.
 * Returns false if there was an error.
 */
bool tok_read(parser_t *t) {
	tokenizer.spaces(t->buf);
    int c = tokenizer.peek(t->buf);
    if (c == EOF) {
        t->next.type = EOF;
    }
    else if (c == '"') {
        readstr(t);
    }
    else if (c == ':' || c == ',' || c == '{' || c == '}' || c == '[' || c == ']') {
        t->next.type = tokenizer.get(t->buf);
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
	if (t->next.type == tok_T_ERR) {
        return false;
    }
    return true;
}

/*
 * Returns the type of the current token.
 */
int tok_currtype(parser_t *l) {
	return l->next.type;
}

/*
 * Returns current token's string value.
 */
const char *tok_currstr(parser_t *l) {
	if (l->next.type != tok_T_STR && l->next.type != tok_T_ERR && l->next.type != tok_T_NUM) {
		return NULL;
	}
	return l->next.str;
}

// Reads a number:
// number = [ minus ] int [ frac ] [ exp ]
void readnum(parser_t *l) {
	tokenizer.t *b = l->buf;
	json_token_t *t = &l->next;

	char buf[100] = {};
	if (!tokenizer.num(b, buf, 100)) {
		seterror(l, "failed to parse a number");
		return;
	}

	resetstr(l);
	for (char *p = buf; *p != '\0'; p++) {
		addchar(l, *p);
	}
	t->type = tok_T_NUM;
}

void readkw(parser_t *l)
{
	tokenizer.t *b = l->buf;
	json_token_t *t = &l->next;
	char kw[8] = {};
	int i = 0;

	while (isalpha(tokenizer.peek(b)) && i < 7) {
		kw[i] = tokenizer.get(b);
		i++;
	}
	kw[i] = '\0';

	switch str (kw) {
		case "true": { t->type = tok_T_TRUE; }
		case "false": { t->type = tok_T_FALSE; }
		case "null": { t->type = tok_T_NULL; }
		default: { seterror(l, "Unknown keyword"); }
	}
}

void readstr(parser_t *l) {
	tokenizer.t *b = l->buf;
	json_token_t *t = &l->next;

	char g = tokenizer.get(b);
	if (g != '"') {
		seterror(l, "'\"' expected");
		return;
	}
	resetstr(l);

	while (tokenizer.more(b) && tokenizer.peek(b) != '"') {
		// Get next string character
		int ch = tokenizer.get(b);
		// If it's a backslash, replace it with the next character.
		if (ch == '\\') {
			ch = tokenizer.get(b);
			if (ch == EOF) {
				seterror(l, "Unexpected end of input");
				return;
			}
		}
		addchar(l, ch);
	}

	g = tokenizer.get(b);
	if (g != '"') {
		seterror(l, "'\"' expected");
		return;
	}
	t->type = tok_T_STR;
}

void seterror(parser_t *l, const char *s) {
	l->next.type = tok_T_ERR;
	putstring(l, "%s", s);
}

void addchar(parser_t *l, int c) {
	if (l->strlen >= l->strcap) {
		l->strcap *= 2;
		l->next.str = realloc(l->next.str, l->strcap);
		if (!l->next.str) {
			panic("realloc failed");
		}
	}
	l->next.str[l->strlen++] = c;
	l->next.str[l->strlen] = '\0';
}

void resetstr(parser_t *l) {
	l->strlen = 0;
}

// Puts a formatted string into the local buffer.
// Returns false in case of error.
bool putstring(parser_t *l, const char *format, ...) {
	va_list args = {};

	// get the total length
	va_start(args, format);
	int len = vsnprintf(NULL, 0, format, args);
	va_end(args);
	if (len < 0) {
		return false;
	}

	// if buffer is smaller, resize the buffer
	if (l->strlen < (size_t)len + 1) {
		size_t newsize = findsize(len + 1);
		l->next.str = realloc(l->next.str, newsize);
		l->strlen = newsize;
		if (l->next.str == NULL) {
			return false;
		}
	}

	// put the string
	va_start(args, format);
	vsnprintf(l->next.str, l->strlen, format, args);
	va_end(args);
	return true;
}

size_t findsize(int len) {
	size_t r = 1;
	size_t n = (size_t) len;
	while (r < n) {
		r *= 2;
	}
	return r;
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
