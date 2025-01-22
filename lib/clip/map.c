pub typedef {
	int size;
	const char *keys[200];
	uint64_t values[200];
	size_t valsize;
} map_t;

// Creates a new map instance.
pub map_t *new(size_t valsize) {
	// Keeping it simple for now.
	if (valsize > sizeof(uint64_t)) {
		panic("value sizes > 8 bytes are not supported");
	}
	map_t *m = calloc(1, sizeof(map_t));
	if (!m) panic("calloc failed");
	m->valsize = valsize;
	return m;
}

// Frees the given instance of map.
pub void free(map_t *m) {
	OS.free(m);
}

// Assigns a value to the given key.
pub void set(map_t *m, const char *key, void *val) {
	int pos = find(m, key);
	if (pos < 0) {
		if (m->size == 200) {
			panic("max map size reached");
		}
		pos = m->size++;
	}
	m->keys[pos] = key;
	memcpy(&m->values[pos], val, m->valsize);
}

// Copies the value stored at the given key into the buffer val.
// If the value doesn't exist, doesn't modify val and returns false.
pub bool get(map_t *m, const char *key, void *val) {
	int pos = find(m, key);
	if (pos < 0) {
		return false;
	}
	memcpy(val, &m->values[pos], m->valsize);
	return m->values[pos];
}

// Returns true if the map has the entry with the given key.
pub bool has(map_t *m, const char *key) {
	return find(m, key) >= 0;
}

int find(map_t *m, const char *key) {
	for (int i = 0; i < m->size; i++) {
		if (!strcmp(key, m->keys[i])) {
			return i;
		}
	}
	return -1;
}

pub typedef { map_t *map; int pos; } iter_t;

pub iter_t *iter(map_t *m) {
	iter_t *it = calloc(1, sizeof(iter_t));
	if (!it) panic("calloc failed");
	it->map = m;
	it->pos = -1;
	return it;
}

pub bool next(iter_t *it) {
	if (it->pos + 1 >= it->map->size) {
		return false;
	}
	it->pos++;
	return true;
}

pub const char *itkey(iter_t *it) {
	return it->map->keys[it->pos];
}

pub void itval(iter_t *it, void *val) {
	memcpy(val, &it->map->values[it->pos], it->map->valsize);
}

pub void end(iter_t *it) {
	OS.free(it);
}
