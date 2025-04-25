enum {
	FSTREAM = 'f',
	BUF = 'b'
}

pub typedef {
	int type;

	// The number of bytes written so far.
	size_t pos;

	// for type == file
	FILE *file_f;

	// for type == buf
	uint8_t *buf_buf;
	size_t buf_size;
} t;

pub t *tobuf(uint8_t *buff, size_t bufsize) {
	t *w = calloc(1, sizeof(t));
	if (!w) panic("calloc failed");
	w->type = BUF;
	w->buf_buf = buff;
	w->buf_size = bufsize;
	return w;
}

pub t *tofile(FILE *f) {
	t *w = calloc(1, sizeof(t));
	if (!w) panic("calloc failed");	
	w->type = FSTREAM;
	w->file_f = f;
	return w;
}

bool byte(t *w, uint8_t b) {
	switch (w->type) {
		case BUF: {
			if (w->pos >= w->buf_size) {
				return false;
			}
			w->buf_buf[w->pos++] = b;
			return true;
		}
		case FSTREAM: {
			if (fputc(b, w->file_f) == EOF) {
				return false;
			}
			w->pos++;
			return true;
		}
	}
	panic("unknown type");
}

pub void free(t *w) {
	OS.free(w);
}

// Starts a list or a dictionary.
pub bool begin(t *w, int type) {
	if (type != 'l' && type != 'd') {
		panic("unknown type");
	}
	return byte(w, type);
}

// Enads a list or a dictionary.
pub bool end(t *w) {
	return byte(w, 'e');
}

pub size_t pos(t *w) {
	return w->pos;
}

pub bool num(t *w, int n) {
	return byte(w, 'i')
		&& _writenum(w, n)
		&& byte(w, 'e');
}

pub bool buf(t *w, const uint8_t *buff, size_t len) {
	if (!_writenum(w, len)) return false;
	if (!byte(w, ':')) return false;
    for (size_t i = 0; i < len; i++) {
		if (!byte(w, buff[i])) return false;
	}
	return true;
}

bool _writenum(t *w, int n) {
	// Write "-" if the number is negative.
	if (n < 0) {
		if (!byte(w, '-')) return false;
		n *= -1;
	}
	size_t N = (size_t) n;

	// Get the number's magnitude.
	size_t m = 1;
	while (m*10 <= N) {
		m *= 10;
	}

	// Write each digit from the biggest magnitude to the smallest.
	while (m > 0) {
		int digit = N / m;
		N = n % m;
		m /= 10;
		if (!byte(w, '0' + digit)) return false;
	}
	return true;
}
