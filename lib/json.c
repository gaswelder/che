#import arr
#import strutil
#import string
#import mem
#import jsontok

// JSON data is represented as a tree where each node is an object of type `json_node`.
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

pub typedef {
	int type;
	union {
		char *str;
		double num;
		arr_t *arr;
		arr_t *obj;
		bool boolval;
	} val;
} json_node;

typedef {
	char *key;
	json_node *val;
} kv_t;

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

// json_node *json_newerror(const char *s) {
// 	json_node *n = newnode(JSON_ERR);
// 	if(!n) return NULL;
// 	n->val.str = newstr("%s", s);
// 	if(!n->val.str) {
// 		free(n);
// 		return NULL;
// 	}
// 	return n;
// }

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
	if (s == NULL) {
		return json_newnull();
	}
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
	kv_t *kv = arr_get(a, i);
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
	kv_t *kv = arr_get(a, i);
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
		kv_t *kv = arr_get(a, i);
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
			kv_t *kv = arr_get(a, i);
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
			char *k = newstr("%s", json_key(obj, i));
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
		copy->val.str = newstr("%s", obj->val.str);
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
	kv_t *kv = NULL;
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
		kv->key = newstr("%s", key);
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
 * Parsing context.
 */
typedef {
	/*
	 * First error reported during parsing.
	 */
	char err[256];
	json_tokenizer_t *lexer;
} parser_t;

/*
 * Parses a given JSON string and returns a pointer to a json_node object.
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
pub json_node *json_parse(const char *s) {
	parser_t p = {};
	p.lexer = new_json_tokenizer(s);
	if (!p.lexer) {
		return NULL;
	}
	if (!lexer_read_next(p.lexer)) {
		error(&p, "%s", p.lexer->next.str);
		free_json_tokenizer(p.lexer);
		return NULL;
	}
	if (p.lexer->next.type == EOF) {
		free_json_tokenizer(p.lexer);
		return NULL;
	}

	json_node *result = read_node(&p);
	if (!result) {
		free_json_tokenizer(p.lexer);
		return NULL;
	}

	// Expect end of file at this point.
	if (p.lexer->next.type != EOF) {
		json_free(result);
		free_json_tokenizer(p.lexer);
		return NULL;
	}
	
	free_json_tokenizer(p.lexer);
	return result;
}

/*
 * Reads one node and returns it.
 * Returns NULL in case of error.
 */
json_node *read_node(parser_t *p)
{
	switch (lexer_curr(p->lexer)->type) {
		case EOF:
			return error(p, "Unexpected end of file");
		case '[':
			return read_array(p);
		case '{':
			return read_dict(p);
		case T_STR:
			json_node *n = json_newstr(lexer_curr(p->lexer)->str);
			lexer_read_next(p->lexer);
			if (lexer_curr(p->lexer)->type == T_ERR) {
				error(p, "%s", lexer_curr(p->lexer)->str);
			}
			return n;
		case T_NUM:
			double n = lexer_curr(p->lexer)->num;
			lexer_read_next(p->lexer);
			if (lexer_curr(p->lexer)->type == T_ERR) {
				error(p, "%s", lexer_curr(p->lexer)->str);
			}
			return json_newnum(n);
		case T_TRUE:
			lexer_read_next(p->lexer);
			return json_newbool(true);
		case T_FALSE:
			lexer_read_next(p->lexer);
			return json_newbool(false);
		case T_NULL:
			lexer_read_next(p->lexer);
			return json_newnull();
	}
	return NULL;
}

json_node *read_array(parser_t *p)
{
	if(!expect(p, '[')) {
		return NULL;
	}

	json_node *a = json_newarr();
	if (lexer_curr(p->lexer)->type == ']') {
		lexer_read_next(p->lexer);
		if (lexer_curr(p->lexer)->type == T_ERR) {
			error(p, "%s", lexer_curr(p->lexer)->str);
		}
		return a;
	}

	while (lexer_curr(p->lexer)->type != EOF) {
		json_node *v = read_node(p);
		if (!v) {
			json_free(a);
			return NULL;
		}
		json_push(a, v);
		if(lexer_curr(p->lexer)->type != ',') {
			break;
		}
		lexer_read_next(p->lexer);
		if (lexer_curr(p->lexer)->type == T_ERR) {
			error(p, "%s", lexer_curr(p->lexer)->str);
		}
	}
	if (lexer_curr(p->lexer)->type != ']') {
		json_free(a);
		return NULL;
	}
	lexer_read_next(p->lexer);
	if (lexer_curr(p->lexer)->type == T_ERR) {
		error(p, "%s", lexer_curr(p->lexer)->str);
	}
	return a;
}

bool consume(parser_t *p, int toktype) {
	if (lexer_curr(p->lexer)->type != toktype) {
		return false;
	}
	lexer_read_next(p->lexer);
	if (lexer_curr(p->lexer)->type == T_ERR) {
		error(p, "%s", lexer_curr(p->lexer)->str);
	}
	return true;
}

json_node *read_dict(parser_t *p)
{
	if (!expect(p, '{')) {
		return NULL;
	}
	json_node *o = json_newobj();
	if (!o) {
		return NULL;
	}
	if (consume(p, '}')) {
		return o;
	}

	while (lexer_curr(p->lexer)->type != EOF) {
		// Get the field name string.
		if (lexer_curr(p->lexer)->type != T_STR) {
			json_free(o);
			return error(p, "Key expected");
		}
		char *key = newstr("%s", lexer_curr(p->lexer)->str);
		if (!key) {
			json_free(o);
			return error(p, "No memory");
		}
		lexer_read_next(p->lexer);

		// Get the ":"
		if (!expect(p, ':')) {
			free(key);
			json_free(o);
			return NULL;
		}

		json_node *val = read_node(p);
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

		if (lexer_curr(p->lexer)->type != ',') {
			break;
		}
		lexer_read_next(p->lexer);
		if (lexer_curr(p->lexer)->type == T_ERR) {
			error(p, "%s", lexer_curr(p->lexer)->str);
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
	if (lexer_curr(p->lexer)->type != toktype) {
		error(p, "'%c' expected, got '%c'", toktype, lexer_curr(p->lexer)->type);
		return false;
	}
	lexer_read_next(p->lexer);
	if (lexer_curr(p->lexer)->type == T_ERR) {
		error(p, "%s", lexer_curr(p->lexer)->str);
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


pub char *json_format(json_node *n)
{
	mem_t *z = memopen();
	json_write(z, n);
	memrewind(z);

	str *s = str_new();
	while(1) {
		char c = memgetc(z);
		if(c == EOF) break;
		str_addc(s, c);
	}
	char *str1 = newstr("%s", str_raw(s));
	str_free(s);
	memclose(z);
	return str1;
}

void json_write(mem_t *z, json_node *n)
{
	switch(json_type(n))
	{
		case JSON_OBJ:
			size_t len = json_size(n);
			memprintf(z, "{");
			for(size_t i = 0; i < len; i++) {
				memprintf(z, "\"%s\":", json_key(n, i));
				json_write(z, json_val(n, i));
				if(i + 1 < len) {
					memprintf(z, ",");
				}
			}
			memprintf(z, "}");
			break;

		case JSON_ARR:
			size_t len = json_len(n);
			memprintf(z, "[");
			for(size_t i = 0; i < len; i++) {
				json_write(z, json_at(n, i));
				if(i + 1 < len) {
					memprintf(z, ",");
				}
			}
			memprintf(z, "]");
			break;

		case JSON_STR:
			write_string(z, n);
			break;

		case JSON_NUM:
			memprintf(z, "%f", json_dbl(n));
			break;

		case JSON_BOOL:
			if (json_bool(n)) {
				memprintf(z, "true");
			} else {
				memprintf(z, "false");
			}
			break;

		case JSON_NULL:
			memprintf(z, "null");
			break;
	}
}

/*
 * Writes a node's escaped string contents.
 */
void write_string(mem_t *z, json_node *node) {
	const char *content = json_str(node);
	
	size_t n = strlen(content);
	memputc('"', z);
	for (size_t i = 0; i < n; i++) {
		const char c = content[i];
		if (c == '\n') {
			memputc('\\', z);
			memputc('n', z);
			continue;
		}
		if (c == '\t') {
			memputc('\\', z);
			memputc('t', z);
			continue;
		}
		if (c == '\\' || c == '\"' || c == '/') {
			memputc('\\', z);
		}
		memputc(c, z);
	}
	memputc('"', z);
}
