struct __arr {
	size_t len;
	size_t maxlen;
	union {
		int i;
		void *v;
	} *vals;
};

typedef struct __arr arr_t;

/*
 * Creates new array
 */
arr_t *arr_new()
{
	return calloc(1, sizeof(arr_t));
}

/*
 * Deletes the array
 */
void arr_free(arr_t *a)
{
	if(a->len > 0) {
		free(a->vals);
	}
	free(a);
}

/*
 * Returns number of elements stored in the array
 */
size_t arr_len(arr_t *a)
{
	return a->len;
}

/*
 * Returns a new copy of the array
 */
arr_t *arr_copy(arr_t *a)
{
	arr_t *c = calloc(1, sizeof(arr_t));
	if(!c) return NULL;

	c->vals = malloc(a->maxlen * sizeof(*c->vals));
	if(!c->vals) {
		free(c);
		return NULL;
	}
	c->maxlen = a->maxlen;

	memcpy(c->vals, a->vals, a->len * sizeof(*a->vals));
	c->len = a->len;
	return c;
}

/*
 * Adds 'val' to the end of the array 'a'.
 * Returns false on failure.
 */
bool arr_push(arr_t *a, void *val)
{
	if(a->len >= a->maxlen && !grow(a)) {
		return false;
	}
	a->vals[a->len].v = val;
	a->len++;
	return true;
}

/*
 * Adds 'val' to the end of the array 'a'.
 * Returns false on failure.
 */
bool arr_pushi(arr_t *a, int val)
{
	if(a->len >= a->maxlen && !grow(a)) {
		return false;
	}
	a->vals[a->len].i = val;
	a->len++;
	return true;
}

/*
 * Returns value at index 'i'.
 */
void *arr_get(arr_t *a, size_t i)
{
	return a->vals[i].v;
}

/*
 * Returns value at index 'i'.
 */
int arr_geti(arr_t *a, size_t i)
{
	return a->vals[i].i;
}

/*
 * Allocates more memory for the contents.
 * Returns false on failure.
 */
static bool grow(struct __arr *a)
{
	size_t newlen = a->len * 2;
	if(!newlen) newlen = 16;

	void *new = realloc(a->vals, sizeof(*a->vals) * newlen);
	if(!new) return false;

	a->vals = new;
	a->maxlen = newlen;
	return true;
}
