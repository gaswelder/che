#import mem

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
 * 'length' is an 64-bit encoding of length of the message.
 * The number of zeros 'z' is such that end of stream happens to be at
 * a length mark that is a multiple of 512 bits (64 bytes).
 */
typedef {
	mem.mem_t *stream; // data stream

	uint64_t length; // current message length in bits
	bool more_data; // set to false after the end of data
	int zeros; // how many zeros left to put out

	uint8_t lenbuf[8]; // buffer with the message length representation
	size_t lenpos; // current position in lenbuf

	bool more;
} src_t;

/*
 * Process the stream and put the digest in 'digest'.
 */
void md5(mem.mem_t *stream, uint32_t digest[4])
{
	src_t s = {
		.stream = stream,
		.length = 0,
		.more_data = true,
		.more = true
	};

	md5_init(digest);

	/*
	 * Process the stream in 16-word blocks.
	 */
	uint32_t block[16] = {0};
	while (s.more)
	{
		for (int i = 0; i < 16; i++) {
			block[i] = next_word(&s);
		}
		//print_block(block);
		md5_feed(digest, block);
	}
}

/*
void print_block(uint32_t b[16])
{
	for(int i = 0; i < 16; i++) {
		printf("%08x (%u)", b[i], b[i]);
		if((i+1) % 4 == 0) putchar('\n');
		else putchar(' ');
	}
}
*/
/*
 * Regardless of the machine, the "md5" byte is big-endian (the highest
 * bit comes first), but a "word", which is 4 bytes, is little-endian
 * (the lowest byte comes first):
 *   |->|->|->|->|->|->|->|->|
 *   |<----------|<----------|
 */
uint32_t next_word(src_t *s)
{
	uint32_t word = 0;
	int pos = 0;
	/*
	 * Compose a word from bytes, least-significant byte first.
	 */
	for(int i = 0; i < 4; i++) {
		uint8_t b = next_byte(s);
		word += (b << pos);
		pos += 8;
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

	assert(s->lenpos < sizeof(s->lenbuf));
	uint8_t b = s->lenbuf[s->lenpos++];

	// Put a flag after the stream is finished.
	if (s->lenpos == sizeof(s->lenbuf)) {
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

	/*
	 * The length mark is a sequence of two words, low word first.
	 */
	int i = 0;
	uint8_t b = 0;
	uint32_t w = 0;
	uint8_t *pos = (uint8_t *) &(s->lenbuf);

	// low word
	w = s->length & 0xFFFFFFFF;
	for(i = 0; i < 4; i++) {
		b = (w >> i*8) & 0xFF;
		*pos = b;
		pos++;
	}
	// high word
	w = (s->length >> 32) & 0xFFFFFFFF;
	for(int i = 0; i < 4; i++) {
		b = (w >> i*8) & 0xFF;
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
 * Data type for the MD5 message digest.
 */
pub typedef uint32_t md5sum_t[4];
pub typedef char md5str_t[33];

/*
 * Computes digest for the string 's' and stores it in 'md'.
 */
pub bool md5_str(const char *s, uint32_t md[4])
{
	return md5_buf(s, strlen(s), md);
}

/*
 * Computes digest for the data in the 'buf' and stores in 'md'.
 */
pub bool md5_buf(const char *buf, size_t len, uint32_t md[4])
{
	mem.mem_t *z = mem.memopen();
	int n = mem.memwrite(z, buf, len);
	mem.memrewind(z);
	assert((size_t) n == len);
	md5(z, md);
	mem.memclose(z);
	return true;
}

/*
 * Returns true if message digests 'a' and 'b' are equal.
 */
pub bool md5_eq(md5sum_t a, b)
{
	for(int i = 0; i < 4; i++) {
		if(a[i] != b[i]) {
			return false;
		}
	}
	return true;
}

pub void md5_print(uint32_t buf[4])
{
	for(int i = 0; i < 4; i++) {
		uint32_t w = buf[i];
		for(int j = 0; j < 4; j++) {
			uint8_t b = (w >> (j * 8)) & 0xFF;
			printf("%02x", b);
		}
	}
}

pub void md5_sprint(md5sum_t s, md5str_t buf)
{
	char *p = buf;
	for(int i = 0; i < 4; i++) {
		uint32_t w = s[i];
		for(int j = 0; j < 4; j++) {
			uint8_t b = (w >> (j * 8)) & 0xFF;
			sprintf(p, "%02x", b);
			p += 2;
		}
	}
}

/*
 * The core routine of the MD5.
 * Defines two main operations: init and feed.
 */

/*
 * https://www.ietf.org/rfc/rfc1321.txt
 * http://stackoverflow.com/a/4124950
 * https://rosettacode.org/wiki/MD5/Implementation
 */

/*
 * 4294967296 * fabs(sin(i)) for i=1..64
 */
uint32_t T[64] = {
	0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
	0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
	0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
	0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
	0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
	0xd62f105d,  0x2441453, 0xd8a1e681, 0xe7d3fbc8,
	0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
	0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
	0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
	0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
	0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x4881d05,
	0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
	0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
	0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
	0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
	0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

/*
 * Initializes the digest
 */
void md5_init(uint32_t md[4])
{
	md[0] = 0x67452301;
	md[1] = 0xEFCDAB89;
	md[2] = 0x98BADCFE;
	md[3] = 0x10325476;
}

/*
 * Feeds next block into the digest
 */
void md5_feed(uint32_t buf[4], uint32_t block[16]) {
	// tmp = buf
	uint32_t tmp[4] = {0};
	tmp[0] = buf[0];
	tmp[1] = buf[1];
	tmp[2] = buf[2];
	tmp[3] = buf[3];

	uint32_t *a = NULL;
	uint32_t *b = NULL;
	uint32_t *c = NULL;
	uint32_t *d = NULL;
	int i = 0;

	/*
	 * 4 vectors, one for each round
	 */
	int V[] = {
		7, 12, 17, 22, // round 1
		5, 9, 14, 20, // round 2
		4, 11, 16, 23, // round 3
		6, 10, 15, 21 // round 4
	};

	for(i = 0; i < 16; i++) {
		int p = 3*i;
		a = &(tmp[(p++)%4]);
		b = &(tmp[(p++)%4]);
		c = &(tmp[(p++)%4]);
		d = &(tmp[(p++)%4]);
		int k = (0+i)%16;
		int *v = V;
		int s = v[i%4];
		int g = i;
		*a = *b + rotate(F(*b, *c, *d) + *a + block[k] + T[g], s);
	}

	for(i = 0; i < 16; i++) {
		int p = 3*i;
		a = &(tmp[(p++)%4]);
		b = &(tmp[(p++)%4]);
		c = &(tmp[(p++)%4]);
		d = &(tmp[(p++)%4]);

		int k = (1+5*i)%16;
		int *v = V + 4;
		int s = v[i%4];
		int g = 16+i;
		*a = *b + rotate(*a + G(*b, *c, *d) + block[k] + T[g], s);
	}

	for(i = 0; i < 16; i++) {
		int p = 3*i;
		a = &(tmp[(p++)%4]);
		b = &(tmp[(p++)%4]);
		c = &(tmp[(p++)%4]);
		d = &(tmp[(p++)%4]);

		int k = (5+3*i)%16;
		int *v = V + 8;
		int s = v[i%4];
		int g = 32+i;
		*a = *b + rotate(*a + H(*b, *c, *d) + block[k] + T[g], s);
	}

	for(i = 0; i < 16; i++) {
		int p = 3*i;
		a = &(tmp[(p++)%4]);
		b = &(tmp[(p++)%4]);
		c = &(tmp[(p++)%4]);
		d = &(tmp[(p++)%4]);

		int k = (7*i)%16;
		int *v = V + 12;
		int s = v[i%4];
		int g = 48+i;
		*a = *b + rotate(*a + I(*b, *c, *d) + block[k] + T[g], s);
	}

	// b "+=" tmp
	buf[0] += tmp[0];
	buf[1] += tmp[1];
	buf[2] += tmp[2];
	buf[3] += tmp[3];
}

uint32_t F(uint32_t x, uint32_t y, uint32_t z) {
	return (x & y) | ((~x) & z);
}
uint32_t G(uint32_t x, uint32_t y, uint32_t z) {
	return (x & z) | (y & (~z));
}
uint32_t H(uint32_t x, uint32_t y, uint32_t z) {
	return x ^ y ^ z;
}
uint32_t I(uint32_t x, uint32_t y, uint32_t z) {
	return y ^ (x | (~z));
}

uint32_t rotate(uint32_t value, int bits) {
	return (value << bits) | (value >> (32-bits));
}
