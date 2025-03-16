#import arr
#import formats/json/tok
#import strbuilder
#import strings

// JSON data is represented as a tree where each node is an object of type `val_t`.
// A node can be of one of the following types:
pub enum {
	JSON_UND = 0,
	JSON_ERR = 1,
	JSON_ARR = 2, // array
	JSON_OBJ = 3, // object
	JSON_STR = 4, // string
	JSON_NUM = 5, // number
	JSON_BOOL = 6, // true or false
	JSON_NULL = 7 // null value
};

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
	if(n->type == JSON_ERR) {
		return n->val.str;
	}
	return NULL;
}

val_t *newnode(int valtype) {
	val_t *n = calloc(1, sizeof(val_t));
	if (!n) {
		panic("failed to allocate memory");
	}
	n->type = valtype;
	return n;
}

pub const char *json_typename(val_t *n)
{
	return typename(n->type);
}

const char *typename(int valtype) {
	switch(valtype) {
		case JSON_UND: { return "und"; }
		case JSON_ERR: { return "err"; }
		case JSON_ARR: { return "arr"; }
		case JSON_OBJ: { return "obj"; }
		case JSON_STR: { return "str"; }
		case JSON_NUM: { return "num"; }
		case JSON_BOOL: { return "bool"; }
		case JSON_NULL: { return "null"; }
	}
	return "?";
}

// val_t *json_newerror(const char *s) {
// 	val_t *n = newnode(JSON_ERR);
// 	if(!n) return NULL;
// 	n->val.str = strings.newstr("%s", s);
// 	if(!n->val.str) {
// 		free(n);
// 		return NULL;
// 	}
// 	return n;
// }

/*
 * Returns a new node of type "object".
 */
pub val_t *json_newobj()
{
	val_t *n = newnode(JSON_OBJ);
	n->val.obj = arr.arr_new();
	if(!n->val.obj) {
		free(n);
		return NULL;
	}
	return n;
}

/*
 * Returns a new node of type "array".
 */
pub val_t *json_newarr()
{
	val_t *n = newnode(JSON_ARR);
	n->val.obj = arr.arr_new();
	if(!n->val.obj) {
		free(n);
		return NULL;
	}
	return n;
}

/*
 * Returns a new node of type "string" with a copy of
 * the given string as the contents.
 */
pub val_t *json_newstr(const char *s)
{
	if (s == NULL) {
		return json_newnull();
	}
	val_t *obj = newnode(JSON_STR);

	char *copy = mcopy( s, strlen(s) + 1 );
	if(!copy) {
		free(obj);
		return NULL;
	}

	obj->val.str = copy;
	return obj;
}

/*
 * Returns a new node of type "number" with the given value
 * as the content.
 */
pub val_t *json_newnum(double val) {
	val_t *obj = newnode(JSON_NUM);
	obj->val.num = val;
	return obj;
}

// Returns a new node of type "null".
pub val_t *json_newnull() {
	return newnode(JSON_NULL);
}

// Returns a new node of type "bool" with the given value.
pub val_t *json_newbool(bool val) {
	val_t *n = newnode(JSON_BOOL);
	n->val.boolval = val;
	return n;
}

// Returns the type of the given node.
pub int type(val_t *obj) {
	if (!obj) return JSON_NULL;
	return obj->type;
}

// Returns the number of keys in an object node.
pub size_t nkeys(val_t *n) {
	if (!n || n->type != JSON_OBJ) {
		return 0;
	}
	return arr.arr_len(n->val.obj);
}

// Returns the i-th key in the object node n.
pub const char *key(val_t *n, size_t i) {
	if (!n || n->type != JSON_OBJ) {
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
	if (!n || n->type != JSON_OBJ) {
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
	if (!n || n->type != JSON_OBJ) {
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
	if (!n || n->type != JSON_ARR) {
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
	if (!obj || obj->type != JSON_ARR) {
		return 0;
	}
	return arr.arr_len(obj->val.arr);
}

// Frees the value.
pub void json_free(val_t *node) {
	if (!node) return;

	if (node->type == JSON_OBJ) {
		arr.arr_t *a = node->val.obj;
		size_t n = arr.arr_len(a);
		for(size_t i = 0; i < n; i++) {
			kv_t *kv = arr.arr_get(a, i);
			free(kv->key);
			json_free(kv->val);
			free(kv);
		}
		arr.arr_free(a);
	} else if (node->type == JSON_ARR) {
		arr.arr_t *a = node->val.obj;
		size_t n = arr.arr_len(a);
		for(size_t i = 0; i < n; i++) {
			json_free(arr.arr_get(a, i));
		}
		arr.arr_free(a);
	}
	else if( node->type == JSON_STR || node->type == JSON_ERR ) {
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

	if( obj->type == JSON_OBJ )
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
	else if( obj->type == JSON_ARR )
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
	else if( obj->type == JSON_STR )
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
	if (!v || v->type != JSON_STR) {
		return NULL;
	}
	return v->val.str;
}

/**
 * Returns the node's value assuming it's a number.
 */
pub double json_getdbl(val_t *obj, const char *k) {
	val_t *v = get(obj, k);
	if (!v || v->type != JSON_NUM) {
		return 0.0;
	}
	return v->val.num;
}

pub bool json_bool(val_t *n)
{
	if(n && n->type == JSON_BOOL) {
		return n->val.boolval;
	}
	return false;
}

// Returns the string from the given string value.
pub const char *json_str(val_t *n) {
	if (n && n->type == JSON_STR) {
		return n->val.str;
	}
	return "";
}

pub double json_dbl(val_t *n)
{
	if(n && n->type == JSON_NUM) {
		return n->val.num;
	}
	return 0.0;
}


pub bool json_put( val_t *n, const char *k, val_t *val )
{
	if(!n || n->type != JSON_OBJ) {
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
		kv = calloc(1, sizeof(*kv));
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
pub bool json_push( val_t *n, val_t *val )
{
	if( !n || n->type != JSON_ARR ) {
		return false;
	}

	return arr.arr_push(n->val.arr, val);
}

void *mcopy( const void *src, size_t size ) {
	void *copy = calloc(size, 1);
	if (!copy) {
		return NULL;
	}
	memcpy(copy, src, size);
	return copy;
}

/*
 * Parsing context.
 */
typedef {
	/*
	 * First error reported during parsing.
	 */
	char err[256];
	tok.json_tokenizer_t *lexer;
} parser_t;

/**
 * Parses a given JSON string and returns a pointer to a val_t object.
 *
 * If the parsing succeeds, the node will be a valid one.
 * In the case of error, a special error node is returned.
 * The user should check the returned node using the `json_error` function.
 * The returned node has to be freed using the `json_free` in both cases.
 * The `json_free` function must be called only on root nodes.
 *
 * There is no need to check the returned value for equality to `NULL`
 * because `json_error` will do that and return the "not enough memory" message
 * (which is the sole cause for the `NULL` return value).
 */
pub val_t *parse(const char *s) {
	parser_t p = {};
	p.lexer = tok.new(s);
	if (!p.lexer) {
		return NULL;
	}
	if (!tok.read(p.lexer)) {
		error(&p, "%s", p.lexer->next.str);
		tok.free(p.lexer);
		return NULL;
	}
	if (p.lexer->next.type == EOF) {
		tok.free(p.lexer);
		return NULL;
	}

	val_t *result = read_node(&p);
	if (!result) {
		tok.free(p.lexer);
		return NULL;
	}

	// Expect end of file at this point.
	if (p.lexer->next.type != EOF) {
		json_free(result);
		tok.free(p.lexer);
		return NULL;
	}
	
	tok.free(p.lexer);
	return result;
}

/*
 * Reads one node and returns it.
 * Returns NULL in case of error.
 */
val_t *read_node(parser_t *p)
{
	switch (tok.currtype(p->lexer)) {
		case EOF: { return error(p, "Unexpected end of file"); }
		case '[': { return read_array(p); }
		case '{': { return read_dict(p); }
		case tok.T_STR: {
			val_t *n = json_newstr(tok.currstr(p->lexer));
			tok.read(p->lexer);
			if (tok.currtype(p->lexer) == tok.T_ERR) {
				error(p, "%s", tok.currstr(p->lexer));
			}
			return n;
		}
		case tok.T_NUM: {
			const char *val = tok.currstr(p->lexer);
			double n = 0;
			if (sscanf(val, "%lf", &n) < 1) {
				error(p, "failed to parser number: %s", val);
			}
			tok.read(p->lexer);
			if (tok.currtype(p->lexer) == tok.T_ERR) {
				error(p, "%s", tok.currstr(p->lexer));
			}
			return json_newnum(n);
		}
		case tok.T_TRUE: {
			tok.read(p->lexer);
			return json_newbool(true);
		}
		case tok.T_FALSE: {
			tok.read(p->lexer);
			return json_newbool(false);
		}
		case tok.T_NULL: {
			tok.read(p->lexer);
			return json_newnull();
		}
	}
	return NULL;
}

val_t *read_array(parser_t *p)
{
	if(!expect(p, '[')) {
		return NULL;
	}

	val_t *a = json_newarr();
	if (tok.currtype(p->lexer) == ']') {
		tok.read(p->lexer);
		if (tok.currtype(p->lexer) == tok.T_ERR) {
			error(p, "%s", tok.currstr(p->lexer));
		}
		return a;
	}

	while (tok.currtype(p->lexer) != EOF) {
		val_t *v = read_node(p);
		if (!v) {
			json_free(a);
			return NULL;
		}
		json_push(a, v);
		if(tok.currtype(p->lexer) != ',') {
			break;
		}
		tok.read(p->lexer);
		if (tok.currtype(p->lexer) == tok.T_ERR) {
			error(p, "%s", tok.currstr(p->lexer));
		}
	}
	if (tok.currtype(p->lexer) != ']') {
		json_free(a);
		return NULL;
	}
	tok.read(p->lexer);
	if (tok.currtype(p->lexer) == tok.T_ERR) {
		error(p, "%s", tok.currstr(p->lexer));
	}
	return a;
}

bool consume(parser_t *p, int toktype) {
	if (tok.currtype(p->lexer) != toktype) {
		return false;
	}
	tok.read(p->lexer);
	if (tok.currtype(p->lexer) == tok.T_ERR) {
		error(p, "%s", tok.currstr(p->lexer));
	}
	return true;
}

val_t *read_dict(parser_t *p)
{
	if (!expect(p, '{')) {
		return NULL;
	}
	val_t *o = json_newobj();
	if (!o) {
		return NULL;
	}
	if (consume(p, '}')) {
		return o;
	}

	while (tok.currtype(p->lexer) != EOF) {
		// Get the field name str.
		if (tok.currtype(p->lexer) != tok.T_STR) {
			json_free(o);
			return error(p, "Key expected");
		}
		char *key = strings.newstr("%s", tok.currstr(p->lexer));
		if (!key) {
			json_free(o);
			return error(p, "No memory");
		}
		tok.read(p->lexer);

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

		if (tok.currtype(p->lexer) != ',') {
			break;
		}
		tok.read(p->lexer);
		if (tok.currtype(p->lexer) == tok.T_ERR) {
			error(p, "%s", tok.currstr(p->lexer));
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
	if (tok.currtype(p->lexer) != toktype) {
		error(p, "'%c' expected, got '%c'", toktype, tok.currtype(p->lexer));
		return false;
	}
	tok.read(p->lexer);
	if (tok.currtype(p->lexer) == tok.T_ERR) {
		error(p, "%s", tok.currstr(p->lexer));
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


pub char *format(val_t *n) {
	strbuilder.str *s = strbuilder.str_new();
	if (!s) {
		return NULL;
	}
	if (!writenode(n, s)) {
		strbuilder.str_free(s);
		return NULL;
	}
	return strbuilder.str_unpack(s);
}

bool writenode(val_t *n, strbuilder.str *s) {
	switch(type(n)) {
        case JSON_OBJ: { return writeobj(n, s); }
        case JSON_ARR: { return writearr(n, s); }
        case JSON_STR: { return writestr(n, s); }
        case JSON_NUM: { return writenum(n, s); }
        case JSON_BOOL: { return writebool(n, s); }
        case JSON_NULL: { return strbuilder.adds(s, "null"); }
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

// ad-hoc function
// write an escaped quoted string s to f.
pub void write_string(FILE *f, char *s) {
	fputc('"', f);
	while (*s) {
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
