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
	uint32_t tmp[4] = {};
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
