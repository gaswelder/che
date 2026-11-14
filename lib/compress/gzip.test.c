#import compress/gzip
#import error
#import mem
#import reader
#import test
#import writer
#import ioutil

int main() {
	lorem();
	roundtrips();
	piperoundtrip(6);
	return test.fails();
}

void lorem() {
	char *expected = "\"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.\"\n";

	FILE *f = fopen("lib/compress/lorem.txt.gz", "rb");
	if (!f) panic("no file");
	reader.t *fr = reader.file(f);
	reader.t *frz = gzip.decompressor(fr);

	size_t len = strlen(expected);
	size_t pos = 0;
	while (true) {
		uint8_t buf[50] = {};
		int r = reader.read(frz, buf, sizeof(buf));
		if (r < 0) {
			test.truth("gzip lorem clean end", r == -1);
			break;
		}
		if (r == 0) {
			test.truth("gzip lorem read", false);
			break;
		}
		if (pos + (size_t) r > len || memcmp(expected + pos, buf, (size_t) r) != 0) {
			printf("gzip lorem mismatch at offset %zu (read %d)\n", pos, r);
			test.truth("gzip lorem stream", false);
			break;
		}
		pos += (size_t) r;
	}
	test.truth("gzip lorem length", pos == len);
	reader.free(frz);
	reader.free(fr);
	fclose(f);
}

// Compress and decompress every data shape at every level, then stream
// one through one-byte input and output slices.
void roundtrips() {
	size_t small[] = { 0, 1, 2, 3, 17, 1000 };
	size_t large[] = { 40000, 200000 };
	for (int32_t level = 1; level <= 9; level++) {
		for (int32_t i = 0; i < 6; i++) {
			roundtrip_shapes(level, small[i], "gzip roundtrip");
		}
	}
	for (int32_t level = 1; level <= 9; level += 4) {
		for (int32_t i = 0; i < 2; i++) {
			roundtrip_shapes(level, large[i], "gzip large roundtrip");
		}
	}
	streaming(9);
}

// Most bytes a gzip stream produces from n input bytes with a single
// final flush: the raw bound plus the fixed header and trailer.
size_t bound(size_t n) {
	uint64_t len = (uint64_t) n;
	uint64_t b = len + len / 8 + 1024 + 18;
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

// Compress the whole input into out with one gzip stream.
bool compress(int32_t level, uint8_t *in, size_t n, uint8_t *out, size_t cap, size_t *produced) {
	writer.t *sink = writer.static_buffer(out, cap);
	writer.t *g = gzip.compressor(sink, level);
	if (!g) {
		writer.free(sink);
		return false;
	}
	error.t err = {};
	write_all(g, in, n);
	gzip.finish(g, &err);
	*produced = sink->nwritten;
	writer.free(g);
	writer.free(sink);
	return !err.set;
}

// Decode a gzip stream that may contain several members.
bool decompress(uint8_t *in, size_t n, uint8_t *out, size_t cap, size_t *produced) {
	reader.t *src = reader.static_buffer(in, n);
	reader.t *r = gzip.decompressor(src);
	bool ok = false;
	if (r) {
		ok = drain_reader(r, out, cap, cap, produced);
		reader.free(r);
	}
	reader.free(src);
	return ok;
}

// A whole-buffer gzip round trip: compress, decompress, and compare.
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
	writer.t *w = gzip.compressor(sink, level);
	test.truth("stream gzip init", w != 0);
	if (!w) {
		writer.free(sink);
		return;
	}

	// One byte at a time through the writer.
	for (size_t i = 0; i < n; i++) {
		int r = writer.write(w, in + i, 1);
		if (r <= 0) {
			test.truth("stream gzip write", false);
			writer.free(w);
			writer.free(sink);
			return;
		}
	}
	error.t ferr = {};
	gzip.finish(w, &ferr);
	writer.free(w);
	size_t clen = sink->nwritten;
	writer.free(sink);
	test.truth("stream gzip finish", !ferr.set);

	// And one byte at a time through the reader.
	reader.t *src = reader.static_buffer(comp, clen);
	reader.t *r = gzip.decompressor(src);
	test.truth("stream gzip decode init", r != 0);
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
	writer.t *enc = gzip.compressor(w, level);
	if (enc) {
		error.t cerr = {};
		if (ioutil.rwpipe(r, enc) < 0) {
			error.set(&cerr, "pipe write failed");
		} else {
			gzip.finish(enc, &cerr);
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
	reader.t *dec = gzip.decompressor(r);
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
