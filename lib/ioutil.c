#import reader
#import writer

// Pipes everything from reader r to writer w.
// Returns the number of bytes written or a negative error code.
pub int64_t rwpipe(reader.t *r, writer.t *w) {
	uint8_t buf[4096];
	int64_t total = 0;
	while (true) {
		int n = reader.read(r, buf, sizeof(buf));
		if (n == EOF) {
			break; // OK
		}
		if (n < 0) {
			return n; // some error
		}
		size_t pos = 0;
		while (pos < (size_t) n) {
			int wr = writer.write(w, buf + pos, (size_t) n - pos);
			if (wr <= 0) {
				return -1;
			}
			pos += (size_t) wr;
			total += (int64_t) wr;
		}
	}
	return total;
}
