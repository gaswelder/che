// zlib (RFC 1950).
// Ported from https://github.com/skeeto/tugz.

#import checksum/adler32
#import compress/deflate
#import compress/inflate
#import error
#import reader
#import writer

// Record a message for an error status in the caller's error sink.
void seterr(error.t *err, int32_t st) {
	if (!err) {
		return;
	}
	switch (st) {
		case deflate.ETRUNC: { error.set(err, "unexpected end of input"); }
		case deflate.EDATA: { error.set(err, "invalid compressed data"); }
		case deflate.EHEADER: { error.set(err, "invalid or unsupported header"); }
		case deflate.ECHECK: { error.set(err, "checksum mismatch"); }
		case deflate.EUSAGE: { error.set(err, "invalid argument or call sequence"); }
	}
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

enum {
	DEC_HEADER,
	DEC_BODY,
	DEC_TRAILER,
	DEC_DONE,
}

typedef {
	inflate.inflator_t *inf;
	int32_t state;
	int32_t err; // sticky container error
	uint32_t check; // Adler-32 of the output so far
	uint64_t total;
	uint8_t buf[10]; // header or trailer bytes
	int32_t len; // bytes in buf
	int32_t hpos; // header bytes consumed, saturated at 2
} inflator_t;

// Start a new stream from any state.
void inflate_reset(inflator_t *z) {
	if (z) {
		inflate.inflate_reset(z->inf);
		z->state = DEC_HEADER;
		z->check = 1;
		z->total = 0;
		z->len = 0;
		z->hpos = 0;
		z->err = 0;
	}
}

// Allocate a decoder state. The caller frees it with inflate_free.
inflator_t *inflate_create() {
	inflator_t *z = calloc!(1, (sizeof(inflator_t)));
	z->inf = inflate.inflate_create();
	inflate_reset(z);
	return z;
}

// Free a decoder state.
void inflate_free(inflator_t *z) {
	if (z) {
		inflate.inflate_free(z->inf);
		free(z);
	}
}

// Consume one header byte.
int32_t header_byte(inflator_t *z, uint8_t c) {
	z->hpos += z->hpos < 2;
	z->buf[z->len++] = c;
	if (z->len != 2) {
		if (z->len < 6) {
			return deflate.DONE; // CMF, or the dictionary ID
		}
		return deflate.EHEADER;
	}
	uint32_t cmf = z->buf[0];
	uint32_t flg = z->buf[1];
	if (((cmf << 8) | flg) % 31) {
		return deflate.EHEADER;
	}
	if ((cmf & 15) != 8) {
		return deflate.EHEADER;
	}
	if (cmf >> 4 > 7) {
		return deflate.EHEADER; // window larger than 32 KiB
	}
	if (flg & 0x20) {
		return deflate.DONE; // a preset dictionary: its ID follows
	}
	z->len = 0;
	z->state = DEC_BODY;
	return deflate.DONE;
}

// Verify the Adler-32 trailer.
int32_t trailer_check(inflator_t *z) {
	if (((uint32_t) z->buf[0] << 24 | (uint32_t) z->buf[1] << 16 | (uint32_t) z->buf[2] << 8 | (uint32_t) z->buf[3]) == z->check) {
		return deflate.DONE;
	}
	return deflate.ECHECK;
}

// Consume only header bytes from b->in.
int32_t decoder_header(inflator_t *z, inflate.zbuf *b) {
	while (!z->err && z->state < DEC_BODY) {
		if (!b->inlen) {
			return deflate.NEED_INPUT;
		}
		uint8_t c = *b->in++;
		b->inlen--;
		int32_t st = header_byte(z, c);
		if (st < deflate.DONE) {
			z->err = st;
		}
	}
	if (z->err) {
		return z->err;
	}
	return deflate.DONE;
}

// Decode from b->in into b->out, advancing both.
int32_t decoder_run(inflator_t *z, inflate.zbuf *b) {
	if (z->err) {
		return z->err;
	}

	while (true) {
		switch (z->state) {
			case DEC_DONE: { return deflate.DONE; }
			case DEC_BODY: {
				// Lengths come from the counts: both pointers may be null
				uint8_t *out = b->out;
				size_t avail = b->outlen;
				int32_t r = inflate.inflate(z->inf, b, 0);
				size_t n = avail - b->outlen;
				z->check = adler32.update(z->check, out, n);
				z->total += (uint64_t) n;
				if (r != deflate.DONE) {
					return r;
				}
				z->state = DEC_TRAILER;
				z->len = 0;
			}
			case DEC_TRAILER: {
				while (z->len < 4) {
					if (!b->inlen) {
						return deflate.NEED_INPUT;
					}
					z->buf[z->len++] = *b->in++;
					b->inlen--;
				}
				int32_t st = trailer_check(z);
				if (st < deflate.DONE) {
					z->err = st;
					return st;
				}
				z->state = DEC_DONE;
			}
			default: {
				int32_t r = decoder_header(z, b);
				if (r != deflate.DONE) {
					return r;
				}
			}
		}
	}
}

// Decode from b->in into b->out, advancing both.
int32_t inflate(inflator_t *z, inflate.zbuf *b, error.t *err) {
	if (!z || !b || (!b->in && b->inlen) || (!b->out && b->outlen)) {
		seterr(err, deflate.EUSAGE);
		return deflate.EUSAGE;
	}
	int32_t st = decoder_run(z, b);
	seterr(err, st);
	return st;
}

typedef {
	deflate.deflator_t *def;
	uint32_t check; // Adler-32 of the input so far
	uint64_t total;
	int32_t done; // trailer written
} deflator_t;

// Start a new stream at a level, which may differ from the previous
// stream's.
void encoder_reset(deflator_t *e, int32_t level) {
	level = max(1, min(level, 9));
	deflate.deflate_reset(e->def, level);
	e->check = 1;
	e->total = 0;
	e->done = 0;

	// Same header as zlib for a 32 KiB window at this level
	uint32_t flevel = 0;
	if (level < 2) {
		flevel = 0;
	} else if (level < 6) {
		flevel = 1;
	} else if (level == 6) {
		flevel = 2;
	} else {
		flevel = 3;
	}
	uint32_t cmf = 0x78;
	uint32_t flg = flevel << 6;
	flg |= 31 - ((cmf << 8) | flg) % 31;
	uint8_t header[2] = { (uint8_t) cmf, (uint8_t) flg };
	deflate.deflate_append(e->def, header, 2);
}

// Allocate an encoder state at a level. Returns null if the level is
// invalid. The caller frees it with deflate_free.
deflator_t *deflate_create(int32_t level) {
	if (level < 1 || level > 9) {
		return 0;
	}
	deflator_t *e = calloc!(1, (sizeof(deflator_t)));
	e->def = deflate.deflate_create(level);
	encoder_reset(e, level);
	return e;
}

// Free an encoder state.
void deflate_free(deflator_t *e) {
	if (e) {
		deflate.deflate_free(e->def);
		free(e);
	}
}

// Compress from b->in into b->out with a flush mode, appending the
// trailer once the raw stream has finished.
int32_t deflate(deflator_t *e, deflate.zbuf *b, int32_t flush, error.t *err) {
	if (!e || !b || (!b->in && b->inlen) || (!b->out && b->outlen) || flush < deflate.NONE || flush > deflate.FINISH) {
		seterr(err, deflate.EUSAGE);
		return deflate.EUSAGE;
	}
	const uint8_t *in = b->in;
	size_t avail = b->inlen;
	int32_t r = deflate.deflate(e->def, b, flush);
	size_t n = avail - b->inlen; // not b->in-in: both may be null
	e->check = adler32.update(e->check, in, n);
	e->total += (uint64_t) n;

	if (flush == deflate.FINISH && r == deflate.DONE && !e->done) {
		uint8_t trailer[4];
		for (int32_t i = 0; i < 4; i++) {
			trailer[i] = (uint8_t)(e->check >> (24 - 8 * i));
		}
		deflate.deflate_append(e->def, trailer, 4);
		e->done = 1;
		r = deflate.deflate(e->def, b, flush);
	}
	seterr(err, r);
	return r;
}

//
// The unzlib reader
//

// Decodes one zlib stream from another reader, on demand. Bytes after
// the stream are ignored.
const int UNZLIB_IN = 4096;
const int UNZLIB_OUT = 4096;

typedef {
	reader.t *in;
	inflator_t *z;
	uint8_t inbuf[UNZLIB_IN];
	uint8_t outbuf[UNZLIB_OUT];
	const uint8_t *incur; // unread compressed bytes in inbuf
	size_t inlen;
	size_t outpos; // next unread decoded byte in outbuf
	size_t outlen;
	bool ended; // the stream has ended
	int32_t status; // how it ended, once it has
} unzlib_t;

// Returns a reader that decodes one zlib stream from in, ignoring
// anything after it. Reads return decoded bytes, then deflate.DONE at the end of
// the stream or the error code that ended it. Free it with reader.free.
pub reader.t *unzlib(reader.t *in) {
	if (!in) {
		return 0;
	}
	unzlib_t *g = calloc!(1, (sizeof(unzlib_t)));
	g->in = in;
	g->z = inflate_create();
	g->incur = g->inbuf;
	return reader.new(g, unzlib_read, unzlib_free);
}

// Frees an unzlib reader's state.
void unzlib_free(void *ctx) {
	unzlib_t *g = ctx;
	if (g) {
		inflate_free(g->z);
		free(g);
	}
}

// Reads up to n decoded bytes to buf. Returns the count read. At the end
// of the stream it returns deflate.DONE, or the error code that ended it.
int unzlib_read(void *ctx, uint8_t *buf, size_t n) {
	unzlib_t *g = ctx;
	if (!n) {
		return 0;
	}
	unzlib_fill(g);
	if (g->outpos == g->outlen) {
		return g->status;
	}
	size_t take = min(g->outlen - g->outpos, n);
	memcpy(buf, g->outbuf + g->outpos, take);
	g->outpos += take;
	return (int) take;
}

// Decode until outbuf holds decoded bytes, the stream ends, or an error
// is hit, recording the end in status.
void unzlib_fill(unzlib_t *g) {
	while (!g->ended && g->outpos == g->outlen) {
		// Feed whatever input is buffered, even none: the decoder may
		// still have buffered bits or trailer bytes to finish with.
		inflate.zbuf b;
		b.in = g->incur;
		b.inlen = g->inlen;
		b.out = g->outbuf;
		b.outlen = (size_t) UNZLIB_OUT;
		int32_t st = inflate(g->z, &b, 0);
		g->incur = b.in;
		g->inlen = b.inlen;
		g->outpos = 0;
		g->outlen = (size_t) UNZLIB_OUT - b.outlen;
		if (st < deflate.DONE) {
			g->ended = true;
			g->status = st;
			return;
		}
		if (st == deflate.DONE) {
			// One stream only: anything after it is ignored.
			g->ended = true;
			g->status = deflate.DONE;
			return;
		}
		if (g->outpos != g->outlen) {
			// Delivered bytes: let the caller read them.
			return;
		}
		if (g->inlen) {
			continue;
		}
		int n = reader.read(g->in, g->inbuf, (size_t) UNZLIB_IN);
		if (n <= 0) {
			// The input ended before the stream did.
			g->ended = true;
			g->status = deflate.ETRUNC;
			return;
		}
		g->incur = g->inbuf;
		g->inlen = (size_t) n;
	}
}

//
// The zlib writer
//

// A writer that encodes what is written to it as one zlib stream,
// forwarding the compressed bytes to another writer. The bytes of each
// write go to the deflator whole: it stages them internally, so the
// writer needs no input buffer of its own. Compressed output is staged
// in ZLIB_OUT bytes.
const int ZLIB_OUT = 4096;

typedef {
	writer.t *out;
	deflator_t *e;
	uint8_t outbuf[ZLIB_OUT];
	bool finished; // the stream has finished
	int32_t status; // how it ended, once it has
} zlibw_t;

// Returns a writer that encodes everything written to it as one zlib
// stream at a level, forwarding the compressed bytes to out. Writes
// return the byte count or -1 on error. Finish the stream with finish,
// which writes the trailer; writes after that fail. Free it with
// writer.free. The wrapped writer stays the caller's.
pub writer.t *zlib(writer.t *out, int32_t level) {
	if (!out) {
		return 0;
	}
	zlibw_t *g = calloc!(1, (sizeof(zlibw_t)));
	g->out = out;
	g->e = deflate_create(level);
	if (!g->e) {
		free(g);
		return 0;
	}
	return writer.new(g, zlibw_write, zlibw_free);
}

// Frees a zlib writer's state.
void zlibw_free(void *ctx) {
	zlibw_t *g = ctx;
	if (g) {
		deflate_free(g->e);
		free(g);
	}
}

// Finishes the zlib writer's stream, writing the trailer. On failure,
// such as a sticky stream error, sets err and leaves the stream ended.
// Finishing an already-finished writer writes nothing more.
pub void finish(writer.t *w, error.t *err) {
	if (!w) {
		panic("null argument");
	}
	zlibw_t *g = w->data;
	if (!g->finished && !g->status) {
		g->finished = true;
		g->status = zlibw_pump(g, 0, 0, deflate.FINISH);
	}
	if (!err || g->status == deflate.DONE) {
		return;
	}
	if (g->status == deflate.NEED_INPUT) {
		error.set(err, "unexpected end of input");
	} else if (g->status < deflate.DONE) {
		seterr(err, g->status);
	} else {
		error.set(err, "stream did not finish");
	}
}

// Writes n uncompressed bytes, encoding them. Returns the byte count or
// -1 on error, including a write once the stream has finished.
int zlibw_write(void *ctx, const uint8_t *data, size_t n) {
	zlibw_t *g = ctx;
	if (!n) {
		return 0;
	}
	if (g->finished || g->status) {
		return -1;
	}
	int32_t st = zlibw_pump(g, data, n, deflate.NONE);
	if (st < deflate.DONE) {
		g->status = st;
		return -1;
	}
	return (int) n;
}

// Run the deflator over n input bytes with a flush mode, writing every
// compressed byte to the wrapped writer as the output staging fills.
// Returns the deflator status.
int32_t zlibw_pump(zlibw_t *g, const uint8_t *data, size_t n, int32_t flush) {
	deflate.zbuf b;
	b.in = data;
	b.inlen = n;
	while (true) {
		b.out = g->outbuf;
		b.outlen = (size_t) ZLIB_OUT;
		int32_t st = deflate(g->e, &b, flush, 0);
		write_all(g->out, g->outbuf, (size_t) ZLIB_OUT - b.outlen);
		if (st == deflate.NEED_OUTPUT) {
			continue;
		}
		return st;
	}
}
