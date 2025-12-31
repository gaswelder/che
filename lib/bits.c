#import reader

// Unpacks bits from a byte into a given array, least significant bit first.
pub void getbits_lsfirst(uint8_t byte, uint8_t *bits) {
	for (int i = 0; i < 8; i++) {
		bits[i] = byte % 2;
		byte /= 2;
	}
}

// Unpacks bits from a byte into a given array, most significant bit first.
pub void getbits_msfirst(uint8_t byte, uint8_t *bits) {
	for (int i = 7; i >= 0; i--) {
		bits[i] = byte % 2;
		byte /= 2;
	}
}

pub typedef {
	reader.t *in;
	uint8_t byte;
	int bytepos;
} reader_t;

// Returns new bitreader reading from r.
pub reader_t *newreader(reader.t *r) {
	reader_t *b = calloc!(1, sizeof(reader_t));
	b->in = r;
	b->bytepos = -1;
	return b;
}

// Frees memory used by reader r.
pub void closereader(reader_t *r) {
	free(r);
}

// Returns the next bit and returns 1 or 0.
// Returns -1 if there is no next bit.
pub int read1(reader_t *s) {
	if (s->bytepos < 0) {
		if (reader.read(s->in, &s->byte, 1) < 0) {
			return -1;
		}
		s->bytepos = 7;
	}
	int b = (s->byte >> s->bytepos) & 1;
	s->bytepos--;
	return b;
}

// Returns value of next n bits.
// Returns -1 if there is not enough bits in the stream.
pub int readn(reader_t *s, int n) {
	int r = 0;
	for (int i = 0; i < n; i++) {
		int c = read1(s);
		if (c < 0) {
			return -1;
		}
		r *= 2;
		r += c;
	}
	return r;
}

pub typedef {
	FILE *f;
	bool err;
	uint8_t buf;
	uint8_t pos;
} writer_t;

pub writer_t *newwriter(FILE *f) {
	writer_t *w = calloc!(1, sizeof(writer_t));
	w->f = f;
	return w;
}

pub bool closewriter(writer_t *w) {
	bool err = w->err;
	if (w->pos > 0 && !err) {
		if (fputc(w->buf, w->f) < 0) {
			err = true;
		}
	}
	free(w);
	return !err;
}

int bitvals[] = {
	1 << 7,
	1 << 6,
	1 << 5,
	1 << 4,
	1 << 3,
	1 << 2,
	1 << 1,
	1 << 0,
};

pub bool writebit(writer_t *w, int bit) {
	if (w->err) return false;
	if (bit) {
		w->buf |= bitvals[w->pos];
	}
	w->pos++;
	if (w->pos == 8) {
		if (fputc(w->buf, w->f) < 0) {
			w->err = true;
			return false;
		}
		w->pos = 0;
		w->buf = 0;
	}
	return true;
}

pub bool write(writer_t *w, uint8_t *bits, size_t nbits) {
	for (size_t i = 0; i < nbits; i++) {
		if (!writebit(w, bits[i])) return false;
	}
	return true;
}
