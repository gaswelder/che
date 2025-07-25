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
	FILE *f;
	uint8_t byte;
	int bytepos;
} bits_t;

/*
 * Returns new bitreader for file 'f'.
 */
pub bits_t *bits_new(FILE *f) {
	bits_t *b = calloc!(1, sizeof(bits_t));
	b->f = f;
	b->bytepos = -1;
	return b;
}

/*
 * Frees memory used by the reader.
 */
pub void bits_free(bits_t *b)
{
	free(b);
}

/*
 * Returns value of the next bit.
 * Returns -1 if there is no next bit.
 */
pub int bits_get(bits_t *s)
{
	if(s->bytepos < 0) {
		int c = fgetc(s->f);
		if(c == EOF) return -1;
		s->byte = (uint8_t) c;
		s->bytepos = 7;
	}
	int b = (s->byte >> s->bytepos) & 1;
	s->bytepos--;
	return b;
}

/*
 * Returns value of next 'n' bits.
 * Returns -1 if there is not enough bits in the stream.
 */
pub int bits_getn(bits_t *s, int n)
{
	int r = 0;
	for(int i = 0; i < n; i++) {
		int c = bits_get(s);
		if(c < 0) {
			return -1;
		}
		r *= 2;
		r += c;
	}
	return r;
}
