#import bytewriter

pub typedef {
	bytewriter.t *bw;
	size_t pos;
} t;

pub t *tobuf(uint8_t *buff, size_t bufsize) {
	t *w = calloc(1, sizeof(t));
	if (!w) return NULL;
	w->bw = bytewriter.tobuf(buff, bufsize);
	if (!w->bw) {
		OS.free(w);
		return NULL;
	}
	return w;
}

pub t *tofile(FILE *f) {
	t *w = calloc(1, sizeof(t));
	if (!w) return NULL;
	w->bw = bytewriter.tofile(f);
	if (!w->bw) {
		OS.free(w);
		return NULL;
	}
	return w;
}

pub void freewriter(t *w) {
	bytewriter.free(w->bw);
	OS.free(w);
}

pub bool begin(t *w, int type) {
	if (type != 'l' && type != 'd') {
		panic("unknown type");
	}
	return byte(w, type);
}

pub size_t pos(t *w) {
	return w->pos;
}

bool byte(t *w, uint8_t b) {
	if (!bytewriter.byte(w->bw, b)) return false;
	w->pos++;
	return true;
}

pub bool end(t *w) {
	return byte(w, 'e');
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
