// Adler-32 checksum (RFC 1950).

const uint32_t MOD = 65521;
const uint32_t NMAX = 5552; // most bytes before the sums may overflow 32 bits

// Updates the checksum from the given buf.
// Start with 1, the checksum of empty input.
pub uint32_t update(uint32_t sum, const uint8_t *buf, size_t len) {
	uint32_t a = sum & 0xffff;
	uint32_t b = sum >> 16;
	while (len) {
		size_t n = min(len, (size_t) NMAX);
		len -= n;
		while (n >= 16) {
			for (int32_t i = 0; i < 16; i++) {
				a += buf[i];
				b += a;
			}
			n -= 16;
			buf += 16;
		}
		while (n) {
			a += *buf++;
			b += a;
			n--;
		}
		a %= MOD;
		b %= MOD;
	}
	return a | (b << 16);
}
