#import mem

pub typedef { uint32_t values[5]; } sha1sum_t;

/*
 * Returns computed digest for the string `s`.
 */
pub sha1sum_t sha1_str(const char *s) {
	return sha1_buf(s, strlen(s));
}

/*
 * Returns computed digest for the data in buffer `buf` of length `n`.
 */
pub sha1sum_t sha1_buf(const char *buf, size_t n) {
	mem.mem_t *z = mem.memopen();
	assert((size_t) mem.memwrite(z, buf, n) == n);
	mem.memrewind(z);

	sha1sum_t r = {};
	sha1(z, r.values);
	mem.memclose(z);
	return r;
}

/*
 * Writes a hexademical representation of the sum `h` to the given buffer `buf`
 * of length `n`. `n` must be at least 41 bytes. Returns false on failure.
 */
pub bool sha1_hex(sha1sum_t h, char *buf, size_t n) {
	if (n <= 40) {
		return false;
	}

	int r = sprintf(buf, "%08x%08x%08x%08x%08x", h.values[0], h.values[1], h.values[2], h.values[3], h.values[4]);
	return r == 40;
}

/*
 * The internal "machinery" processes a stream of 64-byte blocks.
 * The message itself is put in the base of that stream, followed by
 * a padding:
 *
 * | message | eof | zeros | length |
 *
 * Everything is measured in bits, but here we always deal with bytes.
 * 'message' is the actual data, its length is 'b' bits (b/8 bytes).
 * 'eof' byte is a bit '1' followed by seven zero bits.
 * 'zeros' is 'z' zero bytes.
 *
 * 'length' is a 64-bit encoding of length of the message.
 * The number of zeros 'z' is such that end of stream happens to be at
 * a length mark that is a multiple of 512 bits (64 bytes).
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

bool fill_block(src_t *s, uint32_t block[16])
{
	if(!s->more) return false;
	for(int i = 0; i < 16; i++) {
		block[i] = next_word(s);
	}
	return true;
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

void init_padding(src_t *s)
{
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
	 * To get the number of zero bytes we have to solve the following:
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

/*
 * http://csrc.nist.gov/publications/fips/fips180-4/fips-180-4.pdf
 */

void sha1(mem.mem_t *data, uint32_t sum[5])
{
	sum[0] = 0x67452301;
	sum[1] = 0xefcdab89;
	sum[2] = 0x98badcfe;
	sum[3] = 0x10325476;
	sum[4] = 0xc3d2e1f0;

	src_t s = {};
	uint32_t block[16] = {};

	src_init(&s, data);
	while(fill_block(&s, block)) {
		sha1_feed(block, sum);
	}
}

void sha1_feed(uint32_t block[16], uint32_t sum[5])
{
	/*
	 * Prepare message schedule W[t]
	 */
	uint32_t W[80] = {};
	int t = 0;
	for(t = 0; t < 16; t++) {
		W[t] = block[t];
	}
	for(t = 16; t < 80; t++) {
		W[t] = ROTL(1, W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16]);
	}

	uint32_t a = sum[0];
	uint32_t b = sum[1];
	uint32_t c = sum[2];
	uint32_t d = sum[3];
	uint32_t e = sum[4];

	uint32_t T = 0;
	for(t = 0; t < 80; t++) {
		T = ROTL(5, a) + f(t, b, c, d) + e + K(t) + W[t];
		e = d;
		d = c;
		c = ROTL(30, b);
		b = a;
		a = T;
	}

	sum[0] += a;
	sum[1] += b;
	sum[2] += c;
	sum[3] += d;
	sum[4] += e;
}

/*
 * Logical functions f[t]: f0, f1, ..., f79
 */
uint32_t f(int t, uint32_t x, y, z)
{
	if(t < 20) {
		return (x & y) | ((~x) & z);
	}
	if(t < 40) {
		return x ^ y ^ z;
	}
	if(t < 60) {
		return (x & y) ^ (x & z) ^ (y & z);
	}
	return x ^ y ^ z;
}

/*
 * Constants K[i]: K0, K1, ..., K79
 */
uint32_t K(int t)
{
	if(t < 20) {
		return 0x5a827999;
	}
	if(t < 40) {
		return 0x6ed9eba1;
	}
	if(t < 60) {
		return 0x8f1bbcdc;
	}
	return 0xca62c1d6;
}

/*
 * Rotate-left function
 */
uint32_t ROTL(int bits, uint32_t value)
{
	return (value << bits) | (value >> (32 - bits));
}
