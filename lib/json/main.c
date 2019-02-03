import "clip/arr"
import "zio"
import "strutil"

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

typedef struct __json_node json_node;

struct __kv {
	char *key;
	json_node *val;
};

typedef struct __kv kv_t;

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

json_node *json_newerror(const char *s) {
	json_node *n = newnode(JSON_ERR);
	if(!n) return NULL;
	n->val.str = newstr("%s", s);
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
