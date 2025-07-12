pub typedef {
	size_t len;
	size_t maxlen;
	union {
		int i;
		void *v;
	} *vals;
} arr_t;

/*
 * Creates new array
 */
pub arr_t *arr_new()
{
	return calloc!(1, sizeof(arr_t));
}

/*
 * Deletes the array
 */
pub void arr_free(arr_t *a)
{
	if(a->len > 0) {
		free(a->vals);
	}
	free(a);
}

/*
 * Returns number of elements stored in the array
 */
pub size_t arr_len(arr_t *a)
{
	return a->len;
}

/*
 * Returns a new copy of the array
 */
pub arr_t *arr_copy(arr_t *a) {
	arr_t *c = calloc!(1, sizeof(arr_t));
	c->vals = calloc!(a->maxlen, sizeof(*c->vals));
	c->maxlen = a->maxlen;
	memcpy(c->vals, a->vals, a->len * sizeof(*a->vals));
	c->len = a->len;
	return c;
}

/*
 * Adds 'val' to the end of the array 'a'.
 * Returns false on failure.
 */
pub bool arr_push(arr_t *a, void *val)
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
pub bool arr_pushi(arr_t *a, int val)
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
pub void *arr_get(arr_t *a, size_t i)
{
	return a->vals[i].v;
}

/*
 * Returns value at index 'i'.
 */
pub int arr_geti(arr_t *a, size_t i)
{
	return a->vals[i].i;
}

/*
 * Allocates more memory for the contents.
 * Returns false on failure.
 */
bool grow(arr_t *a)
{
	size_t newlen = a->len * 2;
	if(!newlen) newlen = 16;

	void *new = realloc(a->vals, sizeof(*a->vals) * newlen);
	if(!new) return false;

	a->vals = new;
	a->maxlen = newlen;
	return true;
}
