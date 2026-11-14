#import compress/zlib
#import error
#import ioutil
#import mem
#import reader
#import test
#import writer

int main() {
	roundtrips();
	piperoundtrip(6);
	return test.fails();
}

// Compress and decompress every data shape at every level, then stream
// one through one-byte input and output slices.
void roundtrips() {
	size_t small[] = { 0, 1, 2, 3, 17, 1000 };
	size_t large[] = { 40000, 200000 };
	for (int32_t level = 1; level <= 9; level++) {
		for (int32_t i = 0; i < 6; i++) {
			roundtrip_shapes(level, small[i], "zlib roundtrip");
		}
	}
	for (int32_t level = 1; level <= 9; level += 4) {
		for (int32_t i = 0; i < 2; i++) {
			roundtrip_shapes(level, large[i], "zlib large roundtrip");
		}
	}
	streaming(1);
}

// Most bytes a zlib stream produces from n input bytes with a single
// final flush: the raw bound plus the fixed header and trailer.
size_t bound(size_t n) {
	uint64_t len = (uint64_t) n;
	uint64_t b = len + len / 8 + 1024 + 6;
	if (b > (uint64_t)(SIZE_MAX >> 1)) {
		return 0;
	}
	return (size_t) b;
}

// Write all n bytes, panicking on a writer error.
void write_all(writer.t *out, const uint8_t *p, size_t n) {
	size_t pos = 0;
	while (pos < n) {
		int r = writer.write(out, p + pos, n - pos);
		if (r <= 0) {
			panic("tugz: write failed");
		}
		pos += (size_t) r;
	}
}

// Read r into out[0..cap), at most chunk bytes per read. Returns true when
// the reader ended cleanly (a read returned -1) without producing more
// than cap bytes. *produced gets the bytes written.
bool drain_reader(reader.t *r, uint8_t *out, size_t cap, size_t chunk, size_t *produced) {
	size_t bpos = 0;
	if (!chunk) {
		chunk = cap;
	}
	bool ended = false;
	while (bpos < cap) {
		size_t room = cap - bpos;
		if (room > chunk) {
			room = chunk;
		}
		int c = reader.read(r, out + bpos, room);
		if (c <= 0) {
			ended = c == -1;
			break;
		}
		bpos += (size_t) c;
	}
	if (!ended && bpos == cap) {
		// The output filled exactly: ask for the ending without keeping
		// any extra byte. More output than cap is a length error.
		uint8_t extra;
		int c = reader.read(r, &extra, 1);
		ended = c == -1;
	}
	*produced = bpos;
	return ended;
}

// Compress the whole input into out with one zlib stream.
bool compress(int32_t level, uint8_t *in, size_t n, uint8_t *out, size_t cap, size_t *produced) {
	writer.t *sink = writer.static_buffer(out, cap);
	writer.t *z = zlib.zlib(sink, level);
	if (!z) {
		writer.free(sink);
		return false;
	}
	error.t err = {};
	write_all(z, in, n);
	zlib.finish(z, &err);
	*produced = sink->nwritten;
	writer.free(z);
	writer.free(sink);
	return !err.set;
}

// Decode one zlib stream from the whole input.
bool decompress(uint8_t *in, size_t n, uint8_t *out, size_t cap, size_t *produced) {
	reader.t *src = reader.static_buffer(in, n);
	reader.t *r = zlib.unzlib(src);
	bool ok = false;
	if (r) {
		ok = drain_reader(r, out, cap, cap, produced);
		reader.free(r);
	}
	reader.free(src);
	return ok;
}

// A whole-buffer zlib round trip: compress, decompress, and compare.
void roundtrip(int32_t level, uint8_t *in, size_t n, const char *name) {
	size_t cap = bound(n);
	if (!cap) {
		test.truth(name, false);
		return;
	}
	uint8_t *comp = calloc!(1, cap);
	size_t clen = 0;
	if (!compress(level, in, n, comp, cap, &clen)) {
		printf("  compress failed\n");
		test.truth(name, false);
		free(comp);
		return;
	}
	size_t buf = n;
	if (!buf) {
		buf = 1;
	}
	uint8_t *back = calloc!(1, buf);
	size_t blen = 0;
	bool ok = decompress(comp, clen, back, buf, &blen);
	if (!ok || blen != n) {
		printf("  decompress produced %zu of %zu bytes\n", blen, n);
		test.truth(name, false);
		free(comp);
		free(back);
		return;
	}
	test.truth(name, memcmp(in, back, n) == 0);
	free(comp);
	free(back);
}

// Round-trip every sample shape at one size and level.
void roundtrip_shapes(int32_t level, size_t n, const char *name) {
	uint8_t *in = test.sample_mixed(n);
	roundtrip(level, in, n, name);
	free(in);
	in = test.sample_zeros(n);
	roundtrip(level, in, n, name);
	free(in);
	in = test.sample_pattern(n);
	roundtrip(level, in, n, name);
	free(in);
	in = test.sample_random(n);
	roundtrip(level, in, n, name);
	free(in);
	in = test.sample_text(n);
	roundtrip(level, in, n, name);
	free(in);
}

// Stream a compressible buffer through one-byte writes and reads.
void stream(int32_t level, uint8_t *in, size_t n, uint8_t *comp, size_t cap, uint8_t *back) {
	writer.t *sink = writer.static_buffer(comp, cap);
	writer.t *w = zlib.zlib(sink, level);
	test.truth("stream zlib init", w != 0);
	if (!w) {
		writer.free(sink);
		return;
	}

	// One byte at a time through the writer.
	for (size_t i = 0; i < n; i++) {
		int r = writer.write(w, in + i, 1);
		if (r <= 0) {
			test.truth("stream zlib write", false);
			writer.free(w);
			writer.free(sink);
			return;
		}
	}
	error.t ferr = {};
	zlib.finish(w, &ferr);
	writer.free(w);
	size_t clen = sink->nwritten;
	writer.free(sink);
	test.truth("stream zlib finish", !ferr.set);

	// And one byte at a time through the reader.
	reader.t *src = reader.static_buffer(comp, clen);
	reader.t *r = zlib.unzlib(src);
	test.truth("stream zlib decode init", r != 0);
	if (!r) {
		reader.free(src);
		return;
	}
	size_t bpos = 0;
	while (true) {
		int c = reader.read(r, back + bpos, 1);
		if (c <= 0) {
			break;
		}
		bpos += (size_t) c;
	}
	test.truth("stream length", bpos == n);
	test.truth("stream bytes", memcmp(in, back, n) == 0);
	reader.free(r);
	reader.free(src);
}

// Stream a sample through one-byte writes and reads.
void streaming(int32_t level) {
	size_t n = 30000;
	uint8_t *in = test.sample_text(n);
	size_t cap = bound(n);
	uint8_t *comp = calloc!(1, cap);
	uint8_t *back = calloc!(1, n);
	stream(level, in, n, comp, cap, back);
	free(in);
	free(comp);
	free(back);
}

// Compress and decompress a sample through the reader/writer API.
void piperoundtrip(int32_t level) {
	size_t n = 5000;
	uint8_t *in = test.sample_text(n);

	mem.mem_t *comp = mem.memopen();
	reader.t *r = reader.static_buffer(in, n);
	writer.t *w = mem.newwriter(comp);
	bool ok = false;
	writer.t *enc = zlib.zlib(w, level);
	if (enc) {
		error.t cerr = {};
		if (ioutil.rwpipe(r, enc) < 0) {
			error.set(&cerr, "pipe write failed");
		} else {
			zlib.finish(enc, &cerr);
		}
		ok = !cerr.set;
		writer.free(enc);
	}
	reader.free(r);
	writer.free(w);
	test.truth("pipe compress", ok);

	mem.mem_t *back = mem.memopen();
	r = reader.static_buffer((uint8_t *) comp->data, comp->datalen);
	w = mem.newwriter(back);
	ok = false;
	reader.t *dec = zlib.unzlib(r);
	if (dec) {
		int64_t n2 = ioutil.rwpipe(dec, w);
		ok = n2 >= 0;
		reader.free(dec);
	}
	reader.free(r);
	writer.free(w);
	test.truth("pipe decompress", ok);
	test.truth("pipe roundtrip", back->datalen == n && memcmp(in, (uint8_t *) back->data, n) == 0);
	mem.memclose(comp);
	mem.memclose(back);
	free(in);
}
