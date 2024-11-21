#import writer
#import reader

// Writes 1 byte to the writer w.
// It's the same as writing to the writer directly, but it's here for uniformity.
pub int write1(writer.t *w, char b) {
	uint8_t bu = (uint8_t) b;
	return writer.write(w, &bu, 1);
}

// Writes 4 bytes in big-endian order to the writer w.
pub int write4be(writer.t *w, uint32_t v) {
	uint8_t buf[4];
	int pos = 3;
	buf[pos--] = v % 256; v /= 256;
	buf[pos--] = v % 256; v /= 256;
	buf[pos--] = v % 256; v /= 256;
	buf[pos--] = v % 256; v /= 256;
	return writer.write(w, buf, 4);
}

pub int write4le(writer.t *w, uint32_t v) {
	uint8_t buf[4];
	int pos = 0;
	buf[pos++] = v % 256; v /= 256;
	buf[pos++] = v % 256; v /= 256;
	buf[pos++] = v % 256; v /= 256;
	buf[pos++] = v % 256; v /= 256;
	return writer.write(w, buf, 4);
}

pub int write2le(writer.t *w, uint32_t v) {
	uint8_t buf[2];
	int pos = 0;
	buf[pos++] = v % 256; v /= 256;
	buf[pos++] = v % 256; v /= 256;
	return writer.write(w, buf, 2);
}

pub int read1(reader.t *r, uint8_t *v) {
	return reader.read(r, v, 1);
}

// Reads a big-endian uint32 from the reader r.
// Returns the number of bytes read (4) or one of the error values.
pub int read4be(reader.t *r, uint32_t *v) {
	uint8_t buf[4];
	int n = reader.read(r, buf, 4);
	uint32_t x = 0;
	x = x * 256 + buf[0];
	x = x * 256 + buf[1];
	x = x * 256 + buf[2];
	x = x * 256 + buf[3];
	*v = x;
	return n;
}

// Reads a little-endian uint32 from the reader r.
// Returns the number of bytes read or one of the error values.
pub int read4le(reader.t *r, uint32_t *v) {
	uint8_t buf[4];
	int n = reader.read(r, buf, 4);
	uint32_t x = 0;
	x = x * 256 + buf[3];
	x = x * 256 + buf[2];
	x = x * 256 + buf[1];
	x = x * 256 + buf[0];
	*v = x;
	return n;
}

pub int read2le(reader.t *r, uint16_t *v) {
	uint8_t buf[4];
	int n = reader.read(r, buf, 2);
	uint16_t x = 0;
	x = x * 256 + buf[1];
	x = x * 256 + buf[0];
	*v = x;
	return n;
}
