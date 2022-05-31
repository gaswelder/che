#import zio

/*
 * http://csrc.nist.gov/publications/fips/fips180-4/fips-180-4.pdf
 */

void sha1(zio *data, uint32_t sum[5])
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

	uint32_t
		a = sum[0],
		b = sum[1],
		c = sum[2],
		d = sum[3],
		e = sum[4];

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
