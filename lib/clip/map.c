import "cli"
/*
 */

#define MAXKEY 256

/*
 * The map
 */
struct __map {
	struct __table *t;
};

struct __table {
	int nitems; // how many items
	int nbuckets; // how many buckets
	struct __item **buckets;
};

/*
 * An element of the hash table
 */
struct __item {
	struct __item *next;
	char key[MAXKEY];
	void *value;
	int intval;
};

typedef struct __map map;

pub map *map_new()
{
	map *m = alloc(1, sizeof(*m));
	m->t = create_table(16);
	return m;
}

pub void map_free(map *m)
{
	free_table(m->t);
	free(m);
}

/*
 * Returns true if given key exists
 */
pub bool map_exists(map *m, const char *key)
{
	struct __item *i = find(m->t, key);
	return i ? true : false;
}

/*
 * Returns value from given key
 */
pub void *map_get(map *m, const char *key)
{
	struct __item *i = find(m->t, key);
	return i->value;
}

pub int map_geti(map *m, const char *key)
{
	struct __item *i = find(m->t, key);
	return i->intval;
}

/*
 * Assigns value to given key
 */
pub void map_set(map *m, const char *key, void *val)
{
	struct __item *i = find(m->t, key);
	/*
	 * If already exists, just change the value.
	 */
	if(i) {
		i->value = val;
		return;
	}
	/*
	 * Grow the table if needed and insert new item
	 */
	if(m->t->nitems / m->t->nbuckets > 10) {
		grow(m);
	}
	insert(m->t, key, val, 0);
}

pub void map_seti(map *m, const char *key, int val)
{
	struct __item *i = find(m->t, key);
	if(i) {
		i->intval = val;
		return;
	}
	if(m->t->nitems / m->t->nbuckets > 10) {
		grow(m);
	}
	insert(m->t, key, NULL, val);
}

// ---

/*
 * Grows the table
 */
void grow(map *m)
{
	struct __table *old = m->t;

	/*
	 * Create new table and reinsert values into it
	 */
	struct __table *new = create_table(old->nbuckets * 2);
	int j;
	for(j = 0; j < old->nbuckets; j++) {
		struct __item *i = old->buckets[j];
		while(i) {
			insert(new, i->key, i->value, i->intval);
			i = i->next;
		}
	}

	free_table(old);
	m->t = new;
}

/*
 * Finds an item in a single bucket
 */
struct __item *find(struct __table *t, const char *key)
{
	struct __item *i = t->buckets[hash(key) % t->nbuckets];
	while(i) {
		if(strcmp(i->key, key) == 0) {
			return i;
		}
		i = i->next;

	}
	return NULL;
}

void insert(struct __table *t, const char *key, void *val, int intval)
{
	if(strlen(key) >= MAXKEY) {
		fatal("Key too long: %s", key);
	}

	struct __item *i = alloc(1, sizeof(*i));
	strcpy(i->key, key);
	i->value = val;
	i->intval = intval;

	int pos = hash(key) % t->nbuckets;
	struct __item *b = t->buckets[pos];
	i->next = b;
	t->buckets[pos] = i;

	t->nitems++;
}

struct __table *create_table(int size)
{
	struct __table *t = alloc(1, sizeof(*t));
	t->buckets = alloc(size, sizeof(struct __bucket *));
	t->nbuckets = size;
	return t;
}

void free_table(struct __table *t)
{
	int k;
	for(k = 0; k < t->nbuckets; k++) {
		struct __item *i;
		struct __item *next;
		i = t->buckets[k];
		while(i) {
			next = i->next;
			free(i);
			i = next;
		}
	}
	free(t);
}

/*
 * Home-made hash function for strings
 */
int hash(const char *s)
{
	unsigned long h = 0;
	while(*s) {
		h = (h * *s  + 2020181) % 2605013;
		s++;
	}
	return h;
}

void *alloc(size_t n, size_t size)
{
	void *m = calloc(n, size);
	if(!m) {
		fatal("Couldn't allocate %zux%zu bytes", n, size);
	}
	return m;
}
