typedef {
	// Each entry has its own key size.
	size_t keysize;
	uint8_t key[8];

	// Value, the size is constant for the map instance.
	uint8_t value[8];
} entry_t;

pub typedef {
	size_t cap;
	size_t size;
	void *entries;

	// Value size in bytes.
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

	m->cap = 16;
	m->entries = calloc(m->cap, sizeof(entry_t));
	if (!m->entries) panic("calloc failed");

	return m;
}

// Frees the given instance of map.
pub void free(map_t *m) {
	OS.free(m->entries);
	OS.free(m);
}

// Assigns a value to the given key.
pub void set(map_t *m, const char *key, void *val) {
	entry_t *e = find(m, key);
	if (!e) e = create(m, key);
	memcpy(e->value, val, m->valsize);
}

// Copies the value stored at the given key into the buffer val.
// If the value doesn't exist, doesn't modify val and returns false.
pub bool get(map_t *m, const char *key, void *val) {
	entry_t *e = find(m, key);
	if (!e) return false;
	memcpy(val, e->value, m->valsize);
	return true;
}

// Returns true if the map has the entry with the given key.
pub bool has(map_t *m, const char *key) {
	return find(m, key) != NULL;
}

entry_t *find(map_t *m, const char *key) {
	entry_t *ee = m->entries;
	for (size_t i = 0; i < m->size; i++) {
		entry_t *e = &ee[i];
		if (!strcmp(key, (char *)e->key)) {
			return e;
		}
	}
	return NULL;
}

entry_t *create(map_t *m, const char *key) {
	if (strlen(key) > 7) {
		panic("key too long");
	}
	if (m->size == m->cap) {
		m->cap *= 2;
		m->entries = realloc(m->entries, m->cap * sizeof(entry_t));
		if (!m->entries) panic("realloc failed");
	}
	entry_t *ee = m->entries;
	entry_t *e = &ee[m->size++];
	memset(e, 0, sizeof(entry_t));
	memcpy(e->key, key, strlen(key));
	return e;
}

pub typedef { map_t *map; size_t pos; } iter_t;

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
	entry_t *ee = it->map->entries;
	const char *k = (char *)ee[it->pos].key;
	return k;
}

pub void itval(iter_t *it, void *val) {
	entry_t *ee = it->map->entries;
	entry_t *e = &ee[it->pos];
	memcpy(val, e->value, it->map->valsize);
}

pub void end(iter_t *it) {
	OS.free(it);
}
