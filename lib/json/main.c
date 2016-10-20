import "clip/arr"
import "clip/str"
import "parsebuf"
import "zio"

enum {
	JSON_UND,
	JSON_ERR,
	JSON_ARR,
	JSON_OBJ,
	JSON_STR,
	JSON_NUM,
	JSON_BOOL,
	JSON_NULL
};

struct __json_node {
	int type;
	union {
		char *str;
		double num;
		arr_t *arr;
		arr_t *obj;
		bool boolval;
	} val;
};

struct __kv {
	char *key;
	struct __json_node *val;
};

typedef struct __json_node json_node;

pub const char *json_err(json_node *n)
{
	if(n == NULL) {
		return "No memory";
	}
	if(n->type == JSON_ERR) {
		return n->val.str;
	}
	return NULL;
}

json_node *newnode(int type) {
	json_node *n = calloc(1, sizeof(*n));
	if(!n) return NULL;
	n->type = type;
	return n;
}

pub const char *json_typename(json_node *n)
{
	return typename(n->type);
}

pub char *json_format(json_node *n)
{
	zio *z = zopen("mem", "", "");
	json_write(z, n);
	zrewind(z);

	str *s = str_new();
	while(1) {
		char c = zgetc(z);
		if(c == EOF) break;
		str_addc(s, c);
	}
	char *str = strdup(str_raw(s));
	str_free(s);
	zclose(z);
	return str;
}

void json_write(zio *z, json_node *n)
{
	switch(json_type(n))
	{
		case JSON_OBJ:
			size_t len = json_size(n);
			zprintf(z, "{");
			for(size_t i = 0; i < len; i++) {
				zprintf(z, "\"%s\": ", json_key(n, i));
				json_write(z, json_val(n, i));
				if(i + 1 < len) {
					zprintf(z, ",");
				}
			}
			zprintf(z, "}");
			break;

		case JSON_ARR:
			size_t len = json_len(n);
			zprintf(z, "[");
			for(size_t i = 0; i < len; i++) {
				json_write(z, json_at(n, i));
				if(i + 1 < len) {
					zprintf(z, ",");
				}
			}
			zprintf(z, "]");
			break;

		case JSON_STR:
			zprintf(z, "\"%s\"", json_str(n));
			break;

		case JSON_NUM:
			zprintf(z, "%f", json_dbl(n));
			break;

		case JSON_BOOL:
			zprintf(z, json_bool(n)? "true" : "false");
			break;

		case JSON_NULL:
			zprintf(z, "null");
			break;
	}
}

const char *typename(int type) {
	switch(type) {
		case JSON_UND:
			return "und";
		case JSON_ERR:
			return "err";
		case JSON_ARR:
			return "arr";
		case JSON_OBJ:
			return "obj";
		case JSON_STR:
			return "str";
		case JSON_NUM:
			return "num";
		case JSON_BOOL:
			return "bool";
		case JSON_NULL:
			return "null";
	}
	return "?";
}

json_node *json_newerror(const char *s) {
	json_node *n = newnode(JSON_ERR);
	if(!n) return NULL;
	n->val.str = strdup(s);
	if(!n->val.str) {
		free(n);
		return NULL;
	}
	return n;
}

/*
 * Returns a new node of type "object".
 */
pub json_node *json_newobj()
{
	json_node *n = newnode(JSON_OBJ);
	if(!n) return NULL;

	n->val.obj = arr_new();
	if(!n->val.obj) {
		free(n);
		return NULL;
	}
	return n;
}

/*
 * Returns a new node of type "array".
 */
pub json_node *json_newarr()
{
	json_node *n = newnode(JSON_ARR);
	if(!n) return NULL;

	n->val.obj = arr_new();
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
pub json_node *json_newstr(const char *s)
{
	json_node *obj = newnode(JSON_STR);
	if( !obj ) return NULL;

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
pub json_node *json_newnum(double val)
{
	json_node *obj = newnode(JSON_NUM);
	if( !obj ) return NULL;
	obj->val.num = val;
	return obj;
}

/*
 * Returns a new node of type "null".
 */
pub json_node *json_newnull()
{
	return newnode(JSON_NULL);
}

/*
 * Returns a new node of type "bool" with the given value.
 */
pub json_node *json_newbool(bool val)
{
	json_node *o = newnode(JSON_BOOL);
	if(!o) return NULL;

	o->val.boolval = val;
	return o;
}


/*
 * Returns type of the given node.
 */
pub int json_type( json_node *obj )
{
	if( !obj ) {
		return JSON_UND;
	}
	return obj->type;
}

/*
 * Returns number of keys in an object node.
 */
pub size_t json_size(json_node *n)
{
	if(!n || n->type != JSON_OBJ) {
		return 0;
	}
	return arr_len(n->val.obj);
}

/*
 * Returns i-th key in the object node n.
 */
pub const char *json_key(json_node *n, size_t i)
{
	if(!n || n->type != JSON_OBJ) {
		return NULL;
	}
	arr_t *a = n->val.obj;
	size_t len = arr_len(a);
	if(i >= len) return NULL;
	struct __kv *kv = arr_get(a, i);
	return kv->key;
}

/*
 * Returns value stored under i-th key in the object node n.
 */
pub json_node *json_val(json_node *n, size_t i)
{
	if(!n || n->type != JSON_OBJ) {
		return NULL;
	}
	arr_t *a = n->val.obj;
	size_t len = arr_len(a);
	if(i >= len) return NULL;
	struct __kv *kv = arr_get(a, i);
	return kv->val;
}

/*
 * Returns node stored under the given key of the given node.
 * Returns NULL if the given node is not an object or there is no
 * such key in it.
 */
pub json_node *json_get(json_node *n, const char *key)
{
	if(!n || n->type != JSON_OBJ) {
		return NULL;
	}

	arr_t *a = n->val.obj;
	size_t len = arr_len(a);
	for(size_t i = 0; i < len; i++) {
		struct __kv *kv = arr_get(a, i);
		if(strcmp(kv->key, key) == 0) {
			return kv->val;
		}
	}
	return NULL;
}

/*
 * Returns node stored at given index of the given array.
 * Returns NULL if the given node is not an array or the given
 * index is out of bounds.
 */
pub json_node *json_at( json_node *n, int index )
{
	if( !n || n->type != JSON_ARR ) {
		return NULL;
	}
	arr_t *a = n->val.arr;
	if( index < 0 || (size_t) index >= arr_len(a) ) {
		return NULL;
	}
	return arr_get(a, index);
}

/*
 * Returns length of the array object. Returns 0 if the object
 * is not an array.
 */
pub int json_len( json_node *obj )
{
	if( !obj || obj->type != JSON_ARR ) {
		return 0;
	}
	return arr_len(obj->val.arr);
}

/*
 * Free the object.
 */
pub void json_free( json_node *node )
{
	if( !node ) return;

	if( node->type == JSON_OBJ )
	{
		arr_t *a = node->val.obj;
		size_t n = arr_len(a);
		for(size_t i = 0; i < n; i++) {
			struct __kv *kv = arr_get(a, i);
			free(kv->key);
			json_free(kv->val);
			free(kv);
		}
		arr_free(a);
	}
	else if( node->type == JSON_ARR )
	{
		arr_t *a = node->val.obj;
		size_t n = arr_len(a);
		for(size_t i = 0; i < n; i++) {
			json_free(arr_get(a, i));
		}
		arr_free(a);
	}
	else if( node->type == JSON_STR || node->type == JSON_ERR ) {
		free(node->val.str);
	}

	free(node);
}

/*
 * Create a copy of an object.
 */
pub json_node *json_copy( json_node *obj )
{
	if( !obj ) return NULL;

	json_node *copy = mcopy( obj, sizeof(json_node) );
	if( !copy ) {
		return NULL;
	}

	if( obj->type == JSON_OBJ )
	{
		size_t n = json_size(obj);
		for(size_t i = 0; i < n; i++) {
			char *k = strdup(json_key(obj, i));
			json_node *v = json_copy(json_val(obj, i));
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
			json_node *v = json_copy(json_at(obj, i));
			if(!v || !json_push(copy, v)) {
				json_free(v);
				json_free(copy);
				return NULL;
			}
		}
	}
	else if( obj->type == JSON_STR )
	{
		copy->val.str = strdup(obj->val.str);
		if(!copy->val.str) {
			free(copy);
			return NULL;
		}
	}

	return copy;
}

/*
 * Get wrapped values.
 */
pub const char *json_getstr( json_node *obj, const char *key )
{
	json_node *v = json_get( obj, key );
	if( !v || v->type != JSON_STR ) {
		return NULL;
	}
	return v->val.str;
}

pub double json_getdbl( json_node *obj, const char *key )
{
	json_node *v = json_get( obj, key );
	if( !v || v->type != JSON_NUM ) {
		return 0.0;
	}
	return v->val.num;
}

pub bool json_bool(json_node *n)
{
	if(n && n->type == JSON_BOOL) {
		return n->val.boolval;
	}
	return false;
}

pub const char *json_str(json_node *n)
{
	if(n && n->type == JSON_STR) {
		return n->val.str;
	}
	return "";
}

pub double json_dbl(json_node *n)
{
	if(n && n->type == JSON_NUM) {
		return n->val.num;
	}
	return 0.0;
}


pub bool json_put( json_node *n, const char *key, json_node *val )
{
	if(!n || n->type != JSON_OBJ) {
		return false;
	}

	/*
	 * Find if the given key already exists.
	 */
	struct __kv *kv = NULL;
	arr_t *a = n->val.obj;
	size_t len = arr_len(a);
	for(size_t i = 0; i < len; i++) {
		kv = arr_get(a, i);
		if(strcmp(kv->key, key) == 0) {
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
		kv->key = strdup(key);
		arr_push(a, kv);
	}

	kv->val = val;
	return true;
}

/*
 * Pushes given value to the end of the given array node.
 * Returns false if the given node is not an array and in case of error.
 */
pub bool json_push( json_node *n, json_node *val )
{
	if( !n || n->type != JSON_ARR ) {
		return false;
	}

	return arr_push(n->val.arr, val);
}

void *mcopy( const void *src, size_t size )
{
	void *copy = malloc( size );
	if( !copy ) {
		return NULL;
	}
	memcpy( copy, src, size );
	return copy;
}


/*
 * Tokenizer
 */

/*
 * A text will be split into tokens. Most of them
 * are simply characters like '{', but there are
 * also "string" and "number" tokens that have additional
 * value.
 */
struct _token {
	/*
	 * The "type" will be the same as the character in case
	 * of tokens like '{' or ','. For special cases it will
	 * be one of the constants in the enumeration below.
	 */
	int type;
	char *str;
	double num;
};

/*
 * These values are above 255 so that they couldn't be
 * confused with byte values in the parsed string.
 */
enum {
	T_ERR = 257,
	T_TRUE,
	T_FALSE,
	T_NULL,
	T_STR,
	T_NUM
};

/*
 * Parsing context.
 */
struct __parser {
	/*
	 * First error reported during parsing.
	 */
	char err[256];
	parsebuf *buf;
	/*
	 * Look-ahead cache.
	 */
	struct _token next;
};

/*
 * Returns type of the next token
 */
int peek(struct __parser *p)
{
	if(p->next.type == EOF && buf_more(p->buf)) {
		read(p, &(p->next));
	}
	return p->next.type;
}

void get(struct __parser *p, struct _token *t)
{
	/*
	 * Read next value into the cache if necessary.
	 */
	if(p->next.type == EOF) {
		peek(p);
	}

	/*
	 * If t is given, copy the token there.
	 */
	if(t) {
		memcpy(t, &(p->next), sizeof(*t));
	}

	/*
	 * Remove the token from the cache.
	 */
	p->next.type = EOF;
}

void read(struct __parser *p, struct _token *t)
{
	/*
	 * Skip spaces
	 */
	while(isspace(buf_peek(p->buf))) {
		buf_get(p->buf);
	}

	int c = buf_peek(p->buf);
	switch(c)
	{
		case EOF:
			return;
		case '"':
			readstr(p, t);
			return;
		case ':':
		case ',':
		case '{':
		case '}':
		case '[':
		case ']':
			t->type = buf_get(p->buf);
			return;
	}

	if(c == '-' || isdigit(c)) {
		readnum(p, t);
		return;
	}

	if(isalpha(c)) {
		readkw(p, t);
		return;
	}

	t->type = T_ERR;
	error(p, "Unexpected character: %c", c);
}

void readstr(struct __parser *p, struct _token *t)
{
	assert(expectch(p, '"'));
	str *s = str_new();

	while(buf_more(p->buf) && buf_peek(p->buf) != '"') {
		/*
		 * Get next string character
		 */
		int ch = buf_get(p->buf);
		if(ch == '\\') {
			ch = buf_get(p->buf);
			if(ch == EOF) {
				error(p, "Unexpected end of file");
				t->type = T_ERR;
				return;
			}
		}

		/*
		 * Append it to the string
		 */
		if(!str_addc(s, ch)) {
			str_free(s);
			error(p, "Out of memory");
			t->type = T_ERR;
			return;
		}
	}

	if(!expectch(p, '"')) {
		t->type = T_ERR;
		return;
	}

	/*
	 * Create a plain copy of the string
	 */
	char *copy = malloc(str_len(s) + 1);
	if(!copy) {
		free(s);
		error(p, "Out of memory");
		t->type = T_ERR;
		return;
	}

	strcpy(copy, str_raw(s));
	str_free(s);
	t->type = T_STR;
	t->str = copy;
}

bool expectch(struct __parser *p, char c)
{
	char g = buf_get(p->buf);
	if(g != c) {
		error(p, "'%c' expected, got '%c'", c, g);
		return false;
	}
	return true;
}

void readkw(struct __parser *p, struct _token *t)
{
	char kw[8];
	int i = 0;

	while(isalpha(buf_peek(p->buf)) && i < 7) {
		kw[i] = buf_get(p->buf);
		i++;
	}
	kw[i] = '\0';

	if(strcmp(kw, "true") == 0) {
		t->type = T_TRUE;
	}
	else if(strcmp(kw, "false") == 0) {
		t->type = T_FALSE;
	}
	else if(strcmp(kw, "null") == 0) {
		t->type = T_NULL;
	}
	else {
		error(p, "Unknown keyword: %s", kw);
		t->type = T_ERR;
	}
}

/*
 * number = [ minus ] int [ frac ] [ exp ]
 */
void readnum(struct __parser *p, struct _token *t)
{
	bool minus = false;
	double num = 0.0;

	// minus
	if(buf_peek(p->buf) == '-') {
		minus = true;
		buf_get(p->buf);
	}

	// int
	if(!isdigit(buf_peek(p->buf))) {
		error(p, "Digit expected");
		t->type = T_ERR;
		return;
	}
	while(isdigit(buf_peek(p->buf))) {
		num *= 10;
		num += buf_get(p->buf) - '0';
	}

	// frac
	if(buf_peek(p->buf) == '.') {
		buf_get(p->buf);
		if(!isdigit(buf_peek(p->buf))) {
			error(p, "Digit expected");
			t->type = T_ERR;
			return;
		}

		double pow = 0.1;
		while(isdigit(buf_peek(p->buf))) {
			int d = buf_get(p->buf) - '0';
			num += d * pow;
			pow /= 10;
		}
	}

	// exp
	int c = buf_peek(p->buf);
	if(c == 'e' || c == 'E') {
		bool eminus = false;
		buf_get(p->buf);
		c = buf_peek(p->buf);

		// [minus/plus] digit...
		if(c == '-') {
			eminus = true;
			buf_get(p->buf);
		}
		else if(c == '+') {
			eminus = false;
			buf_get(p->buf);
		}

		if(!isdigit(buf_peek(p->buf))) {
			error(p, "Digit expected");
			t->type = T_ERR;
			return;
		}

		int e = 0;
		while(isdigit(buf_peek(p->buf))) {
			e *= 10;
			e += buf_get(p->buf) - '0';
		}

		double mul;
		if(eminus) mul = 0.1;
		else mul = 10;
		while(e-- > 0) {
			num *= mul;
		}
	}

	if(minus) num *= -1;
	t->type = T_NUM;
	t->num = num;
}


/*
 * Puts a formatted error message to the parser's
 * context and returns NULL.
 */
void *error(struct __parser *p, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vsnprintf(p->err, sizeof(p->err), fmt, args);
	va_end(args);
	printf("error: %s\n", p->err);
	return NULL;
}

/*
 * Parses given JSON string and returns a json_node object.
 */
pub json_node *json_parse(const char *s)
{
	struct __parser p;

	p.buf = buf_new(s);
	if(!p.buf) {
		return NULL;
	}
	p.next.type = EOF;

	json_node *n;

	if(peek(&p) == EOF) {
		n = json_newerror("No content");
		buf_free(p.buf);
		return n;
	}

	// <node>
	n = read_node(&p);
	if(!n) {
		n = json_newerror(p.err);
		buf_free(p.buf);
		return n;
	}

	// <eof>
	if(peek(&p) != EOF) {
		json_free(n);
		n = json_newerror("End of file expected");
		buf_free(p.buf);
		return n;
	}

	buf_free(p.buf);
	return n;
}

/*
 * Reads one node and returns it.
 * Returns NULL in case of error.
 */
json_node *read_node(struct __parser *p)
{
	switch(peek(p)) {
		case EOF:
			return error(p, "Unexpected end of file");
		case '[':
			return read_array(p);
		case '{':
			return read_dict(p);
		case T_STR:
			struct _token t;
			get(p, &t);
			json_node *n = json_newstr(t.str);
			free(t.str);
			return n;
		case T_NUM:
			struct _token t;
			get(p, &t);
			return json_newnum(t.num);
		case T_TRUE:
			return json_newbool(true);
		case T_FALSE:
			return json_newbool(false);
		case T_NULL:
			return json_newnull();
	}
	assert(false);
	return NULL;
}

json_node *read_array(struct __parser *p)
{
	if(!expect(p, '[')) {
		return NULL;
	}

	json_node *a = json_newarr();

	if(peek(p) != ']') {
		while(peek(p) != EOF) {
			json_node *v = read_node(p);
			if(!v) {
				json_free(a);
				return NULL;
			}

			json_push(a, v);

			if(peek(p) != ',') {
				break;
			}
			get(p, NULL);
		}
	}
	if(!expect(p, ']')) {
		json_free(a);
		return NULL;
	}

	return a;
}

json_node *read_dict(struct __parser *p)
{
	if(!expect(p, '{')) {
		return NULL;
	}

	json_node *o = json_newobj();
	if(!o) return NULL;

	if(peek(p) != '}') {
		struct _token t;

		while(peek(p) != EOF) {

			if(peek(p) != T_STR) {
				json_free(o);
				return error(p, "Key expected");
			}

			get(p, &t);
			char *key = t.str;

			if(!expect(p, ':')) {
				free(key);
				json_free(o);
				return NULL;
			}

			json_node *val = read_node(p);
			if(!val) {
				free(key);
				json_free(o);
				return NULL;
			}

			if(!json_put(o, key, val)) {
				free(key);
				json_free(val);
				json_free(o);
				return NULL;
			}

			if(peek(p) != ',') {
				break;
			}
			get(p, NULL);
		}
	}

	if(!expect(p, '}')) {
		json_free(o);
		return NULL;
	}

	return o;
}

bool expect(struct __parser *p, int toktype)
{
	struct _token t;
	get(p, &t);
	if(t.type != toktype) {
		error(p, "'%c' expected", toktype);
		return false;
	}
	return true;
}
