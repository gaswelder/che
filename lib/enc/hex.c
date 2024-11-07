#import writer

const char HEX[] = "0123456789abcdef";

pub char *writebyte(uint8_t b, char *p) {
	*p++ = HEX[b/16];
	*p++ = HEX[b%16];
	return p;
}

// Writes n bytes from data to the writer.
// Returns the number of data bytes written or a negative value for errors.
pub int write(writer.t *w, const char *data, size_t n) {
	int len = 0;
	uint8_t two[2];
	for (size_t i = 0; i < n; i++) {
		uint8_t b = (uint8_t) data[i];
		two[0] = HEX[b/16];
		two[1] = HEX[b%16];
		int r = writer.write(w, two, 2);
		if (r < 0) return r; // upstream error
		if (r == 0) break; // out of space
		if (r == 1) return -1; // partial writer, error
		if (r == 2) len++;
	}
	return len;
}
