/*
 * http://csrc.nist.gov/publications/fips/fips180-4/fips-180-4.pdf
 */

#import mem

pub typedef {
	mem.mem_t *z;
	size_t pos;
	bool finished;
	uint32_t sum[5];
} digest_t;

void begin(digest_t *hash) {
	hash->z = mem.memopen();
	if (!hash->z) panic("memopen failed");
	/*
	 * Set the initial sum.
	 */
	hash->sum[0] = 0x67452301;
	hash->sum[1] = 0xefcdab89;
	hash->sum[2] = 0x98badcfe;
	hash->sum[3] = 0x10325476;
	hash->sum[4] = 0xc3d2e1f0;
}

pub bool add(digest_t *hash, char byte) {
	/*
	 * Make sure add is not called after the end.
	 */
	if (hash->finished) return false;
	/*
	 * On the first add allocate memory for the data.
	 */
	if (!hash->z) {
		begin(hash);
	}
	/*
	 * Write to the buffer.
	 */
	mem.memputc(byte, hash->z);
	hash->pos++;
	return true;
}

pub bool end(digest_t *hash) {
	if (hash->finished) return false;
	hash->finished = true;

	if (!hash->z) {
		hash->z = mem.memopen();
		if (!hash->z) panic("memopen failed");
	}

	uint32_t *sum = hash->sum;

	mem.mem_t *data = hash->z;
	mem.memrewind(data);
	src_t s = {};
	src_init(&s, data);

	uint32_t block[16] = {};
	while (s.more) {
		for (int i = 0; i < 16; i++) {
			block[i] = next_word(&s);
		}
		sha1_feed(block, sum);
	}

	mem.memclose(data);

	return true;
}


/*
 * Writes a hexademical representation of the digest to `buf`.
 * bufsize must be at least 41 bytes.
 * Returns false on failure.
 */
pub bool format(digest_t *hash, char *buf, size_t bufsize) {
	// We can work with 40 if we do the formatting here
	// instead of relying on sprintf.
	if (bufsize < 41) return false;

	uint32_t *sum = hash->sum;
	int r = sprintf(buf, "%08x%08x%08x%08x%08x", sum[0], sum[1], sum[2], sum[3], sum[4]);
	return r == 40;
}

/*
 * The internal "machinery" processes a stream of 64-byte blocks.
 * The message is followed by an EOF byte, followed by padding and length:
 *
 * | message | bits "10000000" | padding | length |
 *
 * 'length' is a 64-bit encoding of the message length.
 * The padding is so many zero bytes that the end of stream happens is at
 * a multiple of 64 bytes.
 */
typedef {
	mem.mem_t *stream; // data stream

	uint64_t length; // current message length in bits
	bool more_data; // set to false after the end of data
	int zeros; // how many zeros left to put out

	uint8_t lenbuf[8]; // buffer with the message length representation
	int lenpos; // current position in lenbuf

	bool more;
} src_t;

void src_init(src_t *s, mem.mem_t *data)
{
	s->stream = data;
	s->length = 0;
	s->more_data = true;
	s->more = true;
}

uint32_t next_word(src_t *s)
{
	uint32_t word = 0;
	for(int i = 0; i < 4; i++) {
		uint8_t b = next_byte(s);
		word *= 256;
		word += b;
	}
	return word;
}

uint8_t next_byte(src_t *s)
{
	if(s->more_data) {
		int c = mem.memgetc(s->stream);
		if(c != EOF) {
			s->length += 8;
			return (uint8_t) c;
		}
		s->more_data = false;

		/*
		 * Calculate padding parameters now that we know the
		 * actual message length.
		 */
		init_padding(s);

		/*
		 * The spec says to add bit '1' to the end of the actual
		 * data and then put zeros until a specific position is
		 * reached. We can just add the whole byte with first bit
		 * set to '1' because we will not reach that special
		 * position for at least 7 more bits. And this byte is
		 * actually value 128 (because bytes are treated as big-endian).
		 */
		return 128;
	}

	if(s->zeros > 0) {
		s->zeros--;
		return 0;
	}

	assert((size_t) s->lenpos < sizeof(s->lenbuf));
	uint8_t b = s->lenbuf[s->lenpos++];
	/*
	 * Put a flag after the stream is finished.
	 */
	if(s->lenpos == sizeof(s->lenbuf)) {
		s->more = false;
	}
	return b;
}

void init_padding(src_t *s) {
	/*
	 * Now we know 'b' (data length in bits), so we can create the
	 * length mark and calculate how many zero bytes need to be added.
	 */

	int i = 0;
	uint8_t b = 0;
	uint8_t *pos = (uint8_t *) &(s->lenbuf);
	for(i = 0; i < 8; i++) {
		b = (s->length >> (64-8 - 8*i)) & 0xFF;
		*pos = b;
		pos++;
	}
	s->lenpos = 0;

	/*
	 * (length + 1 + zeros + 8) % 64 == 0
	 * where '1' is for the 'eof' byte and '8' is for the length marker.
	 */
	int l = s->length/8;
	int n = (9 + l) / 64;
	if((9 + l) % 64 != 0) {
		n++;
	}
	s->zeros = 64 * n - 9 - l;
}



/**
 * Feeds the next data block into the current sum.
 */
void sha1_feed(uint32_t block[16], uint32_t sum[5]) {
	/*
	 * Prepare message schedule W[t]
	 */
	uint32_t W[80];
	for (int t = 0; t < 16; t++) W[t] = block[t];
	for (int t = 16; t < 80; t++) W[t] = ROTL(1, W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16]);
	/*
	 * Run the mixer.
	 */
	uint32_t a = sum[0];
	uint32_t b = sum[1];
	uint32_t c = sum[2];
	uint32_t d = sum[3];
	uint32_t e = sum[4];
	uint32_t T = 0;
	for (int t = 0; t < 80; t++) {
		T = ROTL(5, a) + f(t, b, c, d) + e + K(t) + W[t];
		e = d;
		d = c;
		c = ROTL(30, b);
		b = a;
		a = T;
	}
	/*
	 * Update the sum.
	 */
	sum[0] += a;
	sum[1] += b;
	sum[2] += c;
	sum[3] += d;
	sum[4] += e;
}

/*
 * Logical functions f[t]: f0, f1, ..., f79
 */
uint32_t f(int t, uint32_t x, y, z) {
	if (t < 20) return (x & y) | ((~x) & z);
	if (t < 40) return x ^ y ^ z;
	if (t < 60) return (x & y) ^ (x & z) ^ (y & z);
	return x ^ y ^ z;
}

/*
 * Constants K[i]: K0, K1, ..., K79
 */
uint32_t K(int t) {
	if (t < 20) return 0x5a827999;
	if (t < 40) return 0x6ed9eba1;
	if (t < 60) return 0x8f1bbcdc;
	return 0xca62c1d6;
}

/*
 * Rotate-left.
 */
uint32_t ROTL(int bits, uint32_t value) {
	return (value << bits) | (value >> (32 - bits));
}
