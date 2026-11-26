#import writer

// Writes a printf-style string into w.
pub int write(writer.t *w, const char *fmt, ...) {
	// Get the required size.
	va_list args = {};
	va_start(args, fmt);
	int len = vsnprintf(NULL, 0, fmt, args);
	va_end(args);

	// Allocate the buffer.
	uint8_t *buf = calloc!(len + 1, 1);

	// Print into the buffer.
	va_start(args, fmt);
	vsnprintf((char *)buf, len + 1, fmt, args);
	va_end(args);

	int written = writer.write(w, buf, len);
	free(buf);
	return written;
}
