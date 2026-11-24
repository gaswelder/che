#import error

pub typedef {
	const char *pos;
	char *label;
	char *data;
	size_t label_capacity;
	size_t data_capacity;
} reader_t;

// Returns a pem reader for the given pem contents.
// The contents are borrowed.
pub reader_t *new(const char *pem) {
	reader_t *r = calloc!(1, sizeof(reader_t));
	r->pos = pem;
	return r;
}

// Loads the next entry.
// Returns false at EOF or on error.
// EOF leaves err unchanged.
pub bool next(reader_t *r, error.t *err) {
	if (err->set) {
		return false;
	}
	if (r->label) {
		r->label[0] = 0;
	}
	if (r->data) {
		r->data[0] = 0;
	}
	if (!r->pos) {
		return false;
	}
	const char *begin = strstr(r->pos, "-----BEGIN ");
	r->pos = NULL;
	if (!begin) {
		return false;
	}
	begin += strlen("-----BEGIN ");
	const char *label_end = strstr(begin, "-----");
	if (!label_end || label_end == begin) {
		error.set(err, "invalid PEM begin label");
		return false;
	}
	size_t label_len = label_end - begin;
	for (size_t i = 0; i < label_len; i++) {
		if (begin[i] == '\r' || begin[i] == '\n') {
			error.set(err, "newline in PEM label");
			return false;
		}
	}
	const char *body = label_end + 5;
	const char *end = strstr(body, "-----END ");
	if (!end) {
		error.set(err, "missing PEM end boundary");
		return false;
	}
	const char *end_label = end + strlen("-----END ");
	if (strncmp(end_label, begin, label_len) != 0 || strncmp(end_label + label_len, "-----", 5) != 0) {
		error.set(err, "mismatched PEM end boundary");
		return false;
	}
	const char *nested = strstr(body, "-----BEGIN ");
	if (nested && nested < end) {
		error.set(err, "nested PEM begin boundary");
		return false;
	}
	if (r->label_capacity < label_len + 1) {
		r->label = realloc(r->label, label_len + 1);
		if (!r->label) {
			panic("could not allocate PEM label");
		}
		r->label_capacity = label_len + 1;
	}
	size_t data_size = end - body + 1;
	if (r->data_capacity < data_size) {
		r->data = realloc(r->data, data_size);
		if (!r->data) {
			panic("could not allocate PEM contents");
		}
		r->data_capacity = data_size;
	}
	memcpy(r->label, begin, label_len);
	r->label[label_len] = 0;
	size_t len = 0;
	const char *p = body;
	for (; p < end; p++) {
		if (*p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') {
			r->data[len++] = *p;
		}
	}
	r->data[len] = 0;
	r->pos = end_label + label_len + 5;
	return true;
}

pub void close(reader_t *r) {
	free(r->label);
	free(r->data);
	free(r);
}
