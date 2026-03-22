#import reader
#import writer

// Masks to get to i-th bit in a byte,
// assuming 0-th bit is the most significant one.
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

pub enum {
	STRAIGHT, // most-significant bit first
	REVERSED, // least-significant bit first
}

// Unpacks bits from a byte into a given array, least significant bit first.
pub void getbits_lsfirst(uint8_t byte, uint8_t *bits) {
	for (int i = 0; i < 8; i++) {
		bits[i] = (byte & bitvals[7-i]) != 0;
	}
}

// Unpacks bits from a byte into a given array, most significant bit first.
pub void getbits_msfirst(uint8_t byte, uint8_t *bits) {
	for (int i = 7; i >= 0; i--) {
		bits[i] = (byte & bitvals[i]) != 0;
	}
}

//
// reader
//

pub typedef {
	reader.t *in;
	uint8_t byte; // currently loaded byte
	uint8_t rem; // how many bits remain of the byte
	bool reverse; // whether bits are reversed in bytes
} reader_t;

// Returns new bitreader reading from r.
pub reader_t *newreader(reader.t *r, int bitorder) {
	reader_t *b = calloc!(1, sizeof(reader_t));
	b->in = r;
	if (bitorder == REVERSED) {
		b->reverse = true;
	}
	return b;
}

// Frees memory used by reader r.
pub void closereader(reader_t *r) {
	free(r);
}

// Returns the next bit and returns 1 or 0.
// Returns -1 if there is no next bit.
pub int read1(reader_t *s) {
	if (s->rem == 0) {
		if (reader.read(s->in, &s->byte, 1) < 0) {
			return -1;
		}
		s->rem = 8;
	}
	s->rem--;
	int b = 0;
	if (s->reverse) {
		b = s->byte & bitvals[s->rem];
	} else {
		b = s->byte & bitvals[7-s->rem];
	}
	if (b > 0) b = 1;
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

//
// writer
//

pub typedef {
	bool err; // if the writer has encountered an error
	writer.t *out; // writer for completed bytes
	uint8_t buf; // byte being formed
	uint8_t pos; // how many bits have been defined in the byte
	bool reverse; // whether bits are packed into bytes least-significant first
} writer_t;

// Creates a new writer into out.
pub writer_t *newwriter(writer.t *out, int bitorder) {
	writer_t *w = calloc!(1, sizeof(writer_t));
	w->out = out;
	if (bitorder == REVERSED) {
		w->reverse = true;
	}
	return w;
}

void flush(writer_t *w) {
	uint8_t val = w->buf;
	if (w->reverse) {
		val = reversebyte(val);
	}
	if (writer.write(w->out, &val, 1) != 1) {
		w->err = true;
		return;
	}
	w->pos = 0;
	w->buf = 0;
}

// Closes the writer and writes out the unfinished byte.
pub bool closewriter(writer_t *w) {
	if (w->pos > 0 && !w->err) {
		flush(w);
	}
	bool ok = !w->err;
	free(w);
	return ok;
}

// Writes a bit into w.
// Returns true on success and false on error.
pub bool write1(writer_t *w, uint8_t bit) {
	if (w->err) {
		return false;
	}
	if (bit) {
		w->buf |= bitvals[w->pos];
	}
	w->pos++;
	if (w->pos == 8) {
		flush(w);
		if (w->err) {
			return false;
		}
	}
	return true;
}

pub bool write(writer_t *w, uint8_t *bits, size_t nbits) {
	for (size_t i = 0; i < nbits; i++) {
		if (!write1(w, bits[i])) return false;
	}
	return true;
}

// Reverses bits in a byte.
uint8_t reversebyte(uint8_t val) {
	// 0xAA = 10101010
	// 0x55 = 01010101
	// 0xCC = 11001100
	// 0x33 = 00110011
	// 0xF0 = 11110000
	// 0x0F = 00001111
	val = ((val & 0xAA) >> 1) | ((val & 0x55) << 1);
	val = ((val & 0xCC) >> 2) | ((val & 0x33) << 2);
	val = ((val & 0xF0) >> 4) | ((val & 0x0F) << 4);
	return val;
}
