#import enc/hex

/*
 * http://csrc.nist.gov/publications/fips/fips180-4/fips-180-4.pdf
 */

pub typedef {
	bool init;
	size_t pos;
	bool finished;
	uint32_t sum[5];

	uint32_t _block[16];
	size_t _words;
	size_t _bytes;
	uint32_t _word;

	uint64_t length; // current message length in bits.

} digest_t;

pub void reset(digest_t *hash) {
	memset(hash, 0, sizeof(digest_t));
}

/*
 * Writes a hexademical representation of the digest to `buf`.
 * bufsize must be at least 41 bytes.
 * Returns false on failure.
 */
pub bool format(digest_t *hash, char *buf, size_t bufsize) {
	if (bufsize < 40) return false;

	uint8_t bytes[20];
	as_bytes(hash, bytes);

	char *p = buf;
	for (int i = 0; i < 20; i++) {
		p = hex.writebyte(bytes[i], p);
	}
	return true;
}

/**
 * Serializes the digest as a sequence of bytes into buf.
 * The buffer must be 20 bytes long.
 */
pub void as_bytes(digest_t *hash, uint8_t *buf) {
	uint8_t *bp = buf;
	for (int i = 0; i < 5; i++) {
		uint32_t v = hash->sum[i];
		*bp++ = (v >> 24) & 0xFF;
		*bp++ = (v >> 16) & 0xFF;
		*bp++ = (v >> 8) & 0xFF;
		*bp++ = (v >> 0) & 0xFF;
	}
}

pub bool add(digest_t *hash, char byte) {
	// Make sure add is not called after the end.
	if (hash->finished) return false;

	// On the first add set the initial sum.
	if (!hash->init) {
		hash->init = true;
		hash->sum[0] = 0x67452301;
		hash->sum[1] = 0xefcdab89;
		hash->sum[2] = 0x98badcfe;
		hash->sum[3] = 0x10325476;
		hash->sum[4] = 0xc3d2e1f0;
	}

	hash->pos++;
	hash->length += 8;
	push_byte(hash, byte);
	return true;
}

pub bool end(digest_t *hash) {
	if (hash->finished) return false;
	hash->finished = true;

	// The internal "machinery" processes a stream of 64-byte blocks.
	// The message is followed by an EOF byte, followed by padding and length:
	//
	//      message | bits "10000000" | padding | length |
	//
	// 'length' is a 64-bit encoding of the message length.
	// The padding is so many zero bytes that the end of stream happens is at
	// a multiple of 64 bytes.

	// Bits "10000000" from the spec are simply number 128.
	push_byte(hash, 128);

	// (length + 1 + zeros + 8) % 64 == 0
	// where '1' is for the 'eof' byte and '8' is for the length marker.
	int l = hash->length / 8;
	int n = (9 + l) / 64;
	if ((9 + l) % 64 != 0) n++;
	size_t zeros = 64 * n - 9 - l;
	while (zeros > 0) {
		zeros--;
		push_byte(hash, 0);
	}

	// Append the message length.
	for (int i = 0; i < 8; i++) {
		uint8_t b = (hash->length >> (64-8 - 8*i)) & 0xFF;
		push_byte(hash, b);
	}
	return true;
}

void push_byte(digest_t *hash, uint8_t b) {
	uint32_t *sum = hash->sum;

	// Collect a full word.
	// When a full word is collected, push it to the block.
	hash->_word *= 256;
	hash->_word += b;
	hash->_bytes++;
	if (hash->_bytes == 4) {
		hash->_bytes = 0;

		// Collect a full block.
		// When a full block is collected, feed it into the hash.
		hash->_block[hash->_words++] = hash->_word;
		if (hash->_words == 16) {
			hash->_words = 0;
			sha1_feed(hash->_block, sum);
		}
	}
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
