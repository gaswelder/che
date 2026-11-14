#import compress/deflate
#import compress/inflate
#import test

int main() {
	internalflushroundtrip(6, deflate.SYNC);
	internalflushroundtrip(1, deflate.FULL);
	roundtrips();
	return test.fails();
}

// Stress test: compress with periodic flush calls and tiny input and
// output slices, then decode the whole stream.
void internalflushroundtrip(int32_t level, int32_t mode) {
	size_t n = 12000;
	uint8_t *in = test.sample_text(n);
	size_t cap = n * 2 + 8192;
	uint8_t *comp = calloc!(1, cap);
	deflate.deflator_t *d = deflate.deflate_create(level);
	deflate.zbuf b;
	size_t pos = 0;
	size_t opos = 0;
	int32_t chunk = 0;
	int32_t st = 0;
	uint8_t *out = calloc!(1, n);
	size_t produced = 0;

	test.truth("flush init", d != 0);
	while (true) {
		size_t take = 777;
		int32_t flush = deflate.NONE;
		if (take > n - pos) {
			take = n - pos;
		}
		if (pos + take >= n) {
			flush = deflate.FINISH;
		} else if (chunk % 3 == 0) {
			flush = mode;
		}
		b.in = in + pos;
		b.inlen = take;
		b.out = comp + opos;
		b.outlen = 5;
		st = deflate.deflate(d, &b, flush);
		pos += take - b.inlen;
		opos += 5 - b.outlen;
		if (st == deflate.DONE) {
			break;
		}
		if (st == deflate.NEED_OUTPUT) {
			continue;
		}
		if (st == deflate.NEED_INPUT) {
			chunk += 1;
			continue;
		}
		test.truth("flush status", false);
		deflate.deflate_free(d);
		return;
	}
	st = decompress(comp, opos, out, n, &produced);
	test.truth("flush roundtrip", st == deflate.DONE && produced == n && memcmp(in, out, n) == 0);
	deflate.deflate_free(d);
}

// Round-trip streams from every data shape and level, then stream one
// through one-byte input and output slices.
void roundtrips() {
	size_t small[] = { 0, 1, 2, 3, 17, 1000 };
	size_t large[] = { 40000, 200000 };
	for (int32_t level = 1; level <= 9; level++) {
		for (int32_t i = 0; i < 6; i++) {
			roundtrip_shapes(level, small[i], "deflate roundtrip");
		}
	}
	for (int32_t level = 1; level <= 9; level += 4) {
		for (int32_t i = 0; i < 2; i++) {
			roundtrip_shapes(level, large[i], "deflate large roundtrip");
		}
	}
	streaming(6);
}

// Compress the whole input with one raw stream and deflate.FINISH.
int32_t compress(int32_t level, uint8_t *in, size_t n, uint8_t *out, size_t cap, size_t *produced) {
	deflate.deflator_t *d = deflate.deflate_create(level);
	if (!d) {
		return deflate.EUSAGE;
	}
	deflate.zbuf b;
	b.in = in;
	b.inlen = n;
	b.out = out;
	b.outlen = cap;
	int32_t st = deflate.NEED_OUTPUT;
	while (b.outlen) {
		st = deflate.deflate(d, &b, deflate.FINISH);
		if (st != deflate.NEED_OUTPUT) {
			break;
		}
	}
	*produced = cap - b.outlen;
	deflate.deflate_free(d);
	return st;
}

// Decode one raw stream from the whole input.
int32_t decompress(uint8_t *in, size_t n, uint8_t *out, size_t cap, size_t *produced) {
	inflate.inflator_t *z = inflate.inflate_create();
	if (!z) {
		return deflate.EUSAGE;
	}
	inflate.zbuf b;
	b.in = in;
	b.inlen = n;
	b.out = out;
	b.outlen = cap;
	int32_t st = deflate.NEED_OUTPUT;
	while (b.outlen) {
		st = inflate.inflate(z, &b, 0);
		if (st != deflate.NEED_OUTPUT) {
			break;
		}
	}
	*produced = cap - b.outlen;
	inflate.inflate_free(z);
	return st;
}

// A whole-buffer raw round trip: compress, decompress, and compare.
void roundtrip(int32_t level, uint8_t *in, size_t n, const char *name) {
	size_t cap = bound(n);
	if (!cap) {
		test.truth(name, false);
		return;
	}
	uint8_t *comp = calloc!(1, cap);
	size_t clen = 0;
	int32_t st = compress(level, in, n, comp, cap, &clen);
	if (st != deflate.DONE) {
		printf("  compress status: %d\n", st);
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
	st = decompress(comp, clen, back, buf, &blen);
	if (st != deflate.DONE || blen != n) {
		printf("  decompress status: %d, %zu of %zu bytes\n", st, blen, n);
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

// Stream a compressible buffer through one-byte input and output slices,
// then decode it the same way.
void stream(int32_t level, uint8_t *in, size_t n, uint8_t *comp, size_t cap, uint8_t *back) {
	(void) cap;
	deflate.deflator_t *d = deflate.deflate_create(level);
	test.truth("stream deflate init", d != 0);
	if (!d) {
		return;
	}
	size_t pos = 0;
	size_t opos = 0;
	while (true) {
		uint8_t byte = 0;
		const uint8_t *p = 0;
		size_t plen = 0;
		int32_t flush = deflate.NONE;
		if (pos < n) {
			byte = in[pos];
			p = &byte;
			plen = 1;
		} else {
			flush = deflate.FINISH;
		}
		deflate.zbuf b;
		b.in = p;
		b.inlen = plen;
		b.out = comp + opos;
		b.outlen = 1;
		int32_t st = deflate.deflate(d, &b, flush);
		pos += plen - b.inlen;
		opos += 1 - b.outlen;
		if (st == deflate.DONE) {
			break;
		}
		if (st < deflate.DONE) {
			test.truth("stream deflate status", false);
			break;
		}
	}
	deflate.deflate_free(d);

	inflate.inflator_t *z = inflate.inflate_create();
	test.truth("stream inflate init", z != 0);
	if (!z) {
		return;
	}
	size_t ipos = 0;
	size_t bpos = 0;
	int32_t st = inflate.NEED_INPUT;
	while (true) {
		uint8_t byte = 0;
		const uint8_t *p = 0;
		size_t plen = 0;
		if (ipos < opos) {
			byte = comp[ipos];
			p = &byte;
			plen = 1;
		}
		inflate.zbuf b;
		b.in = p;
		b.inlen = plen;
		b.out = back + bpos;
		b.outlen = 1;
		st = inflate.inflate(z, &b, 0);
		ipos += plen - b.inlen;
		bpos += 1 - b.outlen;
		if (st == inflate.DONE) {
			break;
		}
		if (st < inflate.DONE) {
			test.truth("stream inflate status", false);
			break;
		}
		if (st == inflate.NEED_INPUT && ipos == opos) {
			// The input ran out before the stream ended.
			break;
		}
	}
	test.truth("stream inflate status", st == inflate.DONE);
	test.truth("stream length", bpos == n);
	test.truth("stream bytes", memcmp(in, back, n) == 0);
	inflate.inflate_free(z);
}

// Stream a sample through one-byte input and output slices.
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

// A safe upper bound on one raw DEFLATE stream, for sizing test output
// buffers. Fixed Huffman coding expands incompressible bytes by at most
// an eighth; the constant covers block headers and the empty stream.
size_t bound(size_t n) {
	uint64_t len = (uint64_t) n;
	uint64_t b = len + len / 8 + 1024;
	if (b > (uint64_t)(SIZE_MAX >> 1)) {
		return 0;
	}
	return (size_t) b;
}
