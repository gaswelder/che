typedef {
	// Each entry has its own key size.
	size_t keysize;
	uint8_t *key;

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
// valsize specifies the size of the stored values in bytes.
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
	entry_t *ee = m->entries;
	for (size_t i = 0; i < m->size; i++) {
		OS.free(ee[i].key);
	}
	OS.free(m->entries);
	OS.free(m);
}

// Assigns a value to the given key.
pub void set(map_t *m, uint8_t *key, size_t keysize, void *val) {
	// Find or create the entry.
	entry_t *e = find(m, key, keysize);
	if (!e) e = create(m);

	// Set the key.
	e->keysize = keysize;
	e->key = realloc(e->key, e->keysize);
	if (!e->key) panic("key realloc failed");
	memcpy(e->key, key, e->keysize);

	// Set the value.
	memcpy(e->value, val, m->valsize);
}

// Copies the value stored at the given key into the buffer val.
// If the value doesn't exist, doesn't modify val and returns false.
pub bool get(map_t *m, uint8_t *key, size_t keysize, void *val) {
	entry_t *e = find(m, key, keysize);
	if (!e) return false;
	memcpy(val, e->value, m->valsize);
	return true;
}

// Returns true if the map has the entry with the given key.
pub bool has(map_t *m, uint8_t *key, size_t keysize) {
	return find(m, key, keysize) != NULL;
}

pub void sets(map_t *m, const char *key, void *val) {
	set(m, (uint8_t *) key, strlen(key) + 1, val);
}
pub bool gets(map_t *m, const char *key, void *val) {
	return get(m, (uint8_t *) key, strlen(key) + 1, val);
}
pub bool hass(map_t *m, const char *key) {
	return has(m, (uint8_t *) key, strlen(key) + 1);
}

entry_t *find(map_t *m, uint8_t *key, size_t keysize) {
	entry_t *ee = m->entries;
	for (size_t i = 0; i < m->size; i++) {
		entry_t *e = &ee[i];
		if (e->keysize != keysize) continue;
		if (!memcmp(key, e->key, keysize)) {
			return e;
		}
	}
	return NULL;
}

entry_t *create(map_t *m) {
	if (m->size == m->cap) {
		m->cap *= 2;
		m->entries = realloc(m->entries, m->cap * sizeof(entry_t));
		if (!m->entries) panic("realloc failed");
	}
	entry_t *ee = m->entries;
	entry_t *e = &ee[m->size++];
	memset(e, 0, sizeof(entry_t));
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
