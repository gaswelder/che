pub typedef {
	int size;
	const char *keys[200];
	void *values[200];
} map_t;

/*
 * Creates a new map instance.
 */
pub map_t *map_new() {
	map_t *m = calloc(1, sizeof(map_t));
	return m;
}

/*
 * Frees the given instance of map.
 */
pub void map_free(map_t *m) {
	free(m);
}

/*
 * Assigns value to given key
 */
pub void map_set(map_t *m, const char *key, void *val) {
	int pos = find(m, key);
	if (pos < 0) {
		pos = m->size++;
	}
	m->keys[pos] = key;
	m->values[pos] = val;
}

/*
 * Returns true if given key exists.
 */
pub bool map_has(map_t *m, const char *key) {
	return find(m, key) >= 0;
}

/*
 * Returns the value from the given key.
 * Returns null if there is no entry with the given key.
 * If entries can themselves be nulls, use map_has for explicit checks.
 */
pub void *map_get(map_t *m, const char *key) {
	int pos = find(m, key);
	if (pos < 0) {
		return NULL;
	}
	return m->values[pos];
}

int find(map_t *m, const char *key) {
	for (int i = 0; i < m->size; i++) {
		if (!strcmp(key, m->keys[i])) {
			return i;
		}
	}
	return -1;
}

pub typedef { map_t *map; int pos; } map_iter_t;

pub map_iter_t *map_iter(map_t *m) {
	map_iter_t *it = calloc(1, sizeof(map_iter_t));
	it->map = m;
	it->pos = -1;
	return it;
}

pub bool map_iter_next(map_iter_t *it) {
	if (it->pos + 1 >= it->map->size) {
		return false;
	}
	it->pos++;
	return true;
}

pub const char *map_iter_key(map_iter_t *it) {
	return it->map->keys[it->pos];
}

pub void *map_iter_val(map_iter_t *it) {
	return it->map->values[it->pos];
}

pub void map_iter_free(map_iter_t *it) {
	free(it);
}
