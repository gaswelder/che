// gzip (RFC 1952).
// Ported from https://github.com/skeeto/tugz.

#import checksum/crc32
#import compress/deflate
#import compress/inflate
#import error
#import reader
#import writer

// Returns a reader that decodes bytes from reader in.
pub reader.t *decompressor(reader.t *in) {
	if (!in) {
		return 0;
	}
	decompressor_t *g = calloc!(1, (sizeof(decompressor_t)));
	g->in = in;
	g->z = inflate_create();
	g->incur = g->inbuf;
	return reader.new(g, decompressor_read, decompressor_free);
}

// Frees a decompressor reader's state.
void decompressor_free(void *ctx) {
	decompressor_t *g = ctx;
	if (g) {
		inflate_free(g->z);
		free(g);
	}
}

// Reads up to n decoded bytes to buf. Returns the count read. At the end
// of the stream it returns deflate.DONE, or the error code that ended it.
int decompressor_read(void *ctx, uint8_t *buf, size_t n) {
	decompressor_t *g = ctx;
	if (!n) {
		return 0;
	}
	decompressor_fill(g);
	if (g->outpos == g->outlen) {
		return g->status;
	}
	size_t take = min(g->outlen - g->outpos, n);
	memcpy(buf, g->outbuf + g->outpos, take);
	g->outpos += take;
	return (int) take;
}

// Explicit little-endian accesses.
uint32_t load32le(const uint8_t *p) {
	return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24);
}

void store32le(uint8_t *p, uint32_t v) {
	p[0] = (uint8_t) v;
	p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
}

// Record a message for an error status in the caller's error sink.
void seterr(error.t *err, int32_t st) {
	if (!err) {
		return;
	}
	switch (st) {
		case deflate.ETRUNC: { error.set(err, "unexpected end of input"); }
		case deflate.EDATA: { error.set(err, "invalid compressed data"); }
		case deflate.ENOTGZ: { error.set(err, "not gzip data"); }
		case deflate.EHEADER: { error.set(err, "invalid or unsupported header"); }
		case deflate.ECHECK: { error.set(err, "checksum mismatch"); }
		case deflate.ELENGTH: { error.set(err, "length mismatch"); }
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

const int FHCRC = 2;
const int FEXTRA = 4;
const int FNAME = 8;
const int FCOMMENT = 16;

enum {
	DEC_FIXED,   // first 10 header bytes
	DEC_XLEN,
	DEC_EXTRA,
	DEC_NAME,
	DEC_COMMENT,
	DEC_HCRC,
	DEC_BODY,
	DEC_TRAILER,
	DEC_DONE,
}

typedef {
	inflate.inflator_t *inf;
	int32_t state;
	int32_t err; // sticky container error
	uint32_t check; // CRC-32 of the output so far
	uint64_t total;
	uint8_t buf[10]; // header or trailer bytes
	int32_t len; // bytes in buf
	int32_t hpos; // header bytes consumed, saturated at 2
	uint32_t hcrc;
	int32_t flg;
	int32_t xlen;
} inflator_t;

// Start a new member from any state.
void inflate_reset(inflator_t *z) {
	if (z) {
		inflate.inflate_reset(z->inf);
		z->state = DEC_FIXED;
		z->check = 0;
		z->total = 0;
		z->len = 0;
		z->hpos = 0;
		z->hcrc = 0;
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

// Advance the header state past fields the flags say are absent.
void skip_absent(inflator_t *z) {
	while (true) {
		switch (z->state) {
			case DEC_XLEN: {
				if (z->flg & FEXTRA) {
					return;
				}
			}
			case DEC_EXTRA: {
				if (z->xlen) {
					return;
				}
			}
			case DEC_NAME: {
				if (z->flg & FNAME) {
					return;
				}
			}
			case DEC_COMMENT: {
				if (z->flg & FCOMMENT) {
					return;
				}
			}
			case DEC_HCRC: {
				if (z->flg & FHCRC) {
					return;
				}
			}
			default: { return; }
		}
		z->state++;
	}
}

// Consume one byte of a fixed-size header field: the first 10 bytes,
// XLEN, or the header CRC.
int32_t header_byte(inflator_t *z, uint8_t c) {
	z->hpos += z->hpos < 2;
	switch (z->state) {
		case DEC_FIXED: {
			// Each field is validated as soon as it is complete, like zlib.
			// As in GNU gzip, the old magic, 1f 9e, also begins a member.
			z->buf[z->len++] = c;
			if (z->len == 2 && (z->buf[0] != 0x1f || z->buf[1] != 0x8b && z->buf[1] != 0x9e)) {
				return deflate.ENOTGZ;
			}
			if (z->len == 4 && z->buf[2] != 8) {
				return deflate.EHEADER;
			}
			if (z->len == 4 && z->buf[3] & 0xe0) {
				return deflate.EHEADER;
			}
			if (z->len < 10) {
				return deflate.DONE;
			}
			z->flg = z->buf[3];
			if (z->flg & FHCRC) {
				z->hcrc = crc32.update(0, z->buf, 10);
			}
			z->len = 0;
			z->xlen = 0;
			z->state = DEC_XLEN;
		}
		case DEC_XLEN: {
			if (z->flg & FHCRC) {
				z->hcrc = crc32.update(z->hcrc, &c, 1);
			}
			z->xlen |= (int32_t) c << (8 * z->len);
			z->len++;
			if (z->len < 2) {
				return deflate.DONE;
			}
			z->len = 0;
			z->state = DEC_EXTRA;
		}
		case DEC_HCRC: {
			z->buf[z->len++] = c;
			if (z->len < 2) {
				return deflate.DONE;
			}
			if (((int32_t) z->buf[0] | ((int32_t) z->buf[1] << 8)) != (int32_t)(z->hcrc & 0xffff)) {
				return deflate.EHEADER;
			}
			z->state = DEC_BODY;
		}
	}
	skip_absent(z);
	return deflate.DONE;
}

// Consume as much of a variable header field (the extra field, the name,
// or the comment) as b->in holds, at least a byte, in one span.
void header_span(inflator_t *z, inflate.zbuf *b) {
	size_t n = 0;
	int32_t done = 0;
	if (z->state == DEC_EXTRA) {
		n = min((size_t) z->xlen, (size_t) b->inlen);
		z->xlen -= (int32_t) n;
		if (!z->xlen) {
			done = 1;
		}
	} else {
		for (; n < b->inlen && b->in[n]; n++) {
		}
		if (n < b->inlen) {
			done = 1;
		}
		if (done) {
			n++; // the terminating zero
		}
	}
	if (z->flg & FHCRC) {
		z->hcrc = crc32.update(z->hcrc, b->in, n);
	}
	z->hpos = (int32_t) min((size_t) z->hpos + min(n, 2), 2);
	b->in += n;
	b->inlen -= n;
	if (done) {
		z->state++;
		skip_absent(z);
	}
}

// Verify each trailer field as soon as it is complete, like zlib.
int32_t trailer_check(inflator_t *z) {
	if (z->len == 4) {
		if (load32le(z->buf) == z->check) {
			return deflate.DONE;
		}
		return deflate.ECHECK;
	}
	if (load32le(z->buf + 4) == (uint32_t) z->total) {
		return deflate.DONE;
	}
	return deflate.ELENGTH;
}

// Consume only header bytes from b->in.
int32_t decoder_header(inflator_t *z, inflate.zbuf *b) {
	while (!z->err && z->state < DEC_BODY) {
		if (!b->inlen) {
			return deflate.NEED_INPUT;
		}
		if (z->state >= DEC_EXTRA && z->state <= DEC_COMMENT) {
			header_span(z, b);
			continue;
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
	if (z->state == DEC_DONE && b->inlen) {
		inflate_reset(z);
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
				z->check = crc32.update(z->check, out, n);
				z->total += (uint64_t) n;
				if (r != deflate.DONE) {
					return r;
				}
				z->state = DEC_TRAILER;
				z->len = 0;
			}
			case DEC_TRAILER: {
				while (z->len < 8) {
					if (!b->inlen) {
						return deflate.NEED_INPUT;
					}
					z->buf[z->len++] = *b->in++;
					b->inlen--;
					if (z->len == 4 || z->len == 8) {
						int32_t st = trailer_check(z);
						if (st < deflate.DONE) {
							z->err = st;
							return st;
						}
					}
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

//
// The decompressor reader
//

// A reader that decodes the gzip members of another reader. Reads pull
// compressed bytes from the underlying reader and decode them on
// demand, one or more members back to back.
const int DECOMPRESSOR_IN = 4096;
const int DECOMPRESSOR_OUT = 4096;

typedef {
	reader.t *in;
	inflator_t *z;
	uint8_t inbuf[DECOMPRESSOR_IN];
	uint8_t outbuf[DECOMPRESSOR_OUT];
	const uint8_t *incur; // unread compressed bytes in inbuf
	size_t inlen;
	size_t outpos; // next unread decoded byte in outbuf
	size_t outlen;
	bool ended; // the stream has ended
	int32_t status; // how it ended, once it has
} decompressor_t;



// Decode until outbuf holds decoded bytes, the stream ends, or an error
// is hit, recording the end in status.
void decompressor_fill(decompressor_t *g) {
	while (!g->ended && g->outpos == g->outlen) {
		// Feed whatever input is buffered, even none: the decoder may
		// still have buffered bits or trailer bytes to finish with.
		inflate.zbuf b;
		b.in = g->incur;
		b.inlen = g->inlen;
		b.out = g->outbuf;
		b.outlen = (size_t) DECOMPRESSOR_OUT;
		int32_t st = inflate(g->z, &b, 0);
		g->incur = b.in;
		g->inlen = b.inlen;
		g->outpos = 0;
		g->outlen = (size_t) DECOMPRESSOR_OUT - b.outlen;
		if (st < deflate.DONE) {
			g->ended = true;
			g->status = st;
			return;
		}
		if (g->outpos != g->outlen) {
			// Delivered bytes: let the caller read them.
			return;
		}
		if (st == deflate.DONE && g->inlen) {
			// Another member begins in the next pass.
			continue;
		}
		// At a member boundary or short of input, look for more.
		int n = reader.read(g->in, g->inbuf, (size_t) DECOMPRESSOR_IN);
		if (n <= 0) {
			// The input ended: at a member boundary the stream is
			// complete, and anywhere else it ended early.
			g->ended = true;
			g->status = deflate.ETRUNC;
			if (g->z->state == DEC_DONE) {
				g->status = deflate.DONE;
			}
			return;
		}
		g->incur = g->inbuf;
		g->inlen = (size_t) n;
	}
}

typedef {
	deflate.deflator_t *def;
	uint32_t check; // CRC-32 of the input so far
	uint64_t total;
	int32_t done; // trailer written
} deflator_t;

// Start a new stream at a level, which may differ from the previous
// stream's.
void encoder_reset(deflator_t *e, int32_t level) {
	level = max(1, min(level, 9));
	deflate.deflate_reset(e->def, level);
	e->check = 0;
	e->total = 0;
	e->done = 0;

	// XFL marks the fastest and the smallest levels, as GNU gzip and
	// zlib mark them
	uint8_t header[10] = { 0x1f, 0x8b, 8, 0, 0, 0, 0, 0, 0, 3 };
	switch (level) {
		case 1: { header[8] = 4; }
		case 9: { header[8] = 2; }
	}
	deflate.deflate_append(e->def, header, 10);
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
	e->check = crc32.update(e->check, in, n);
	e->total += (uint64_t) n;

	if (flush == deflate.FINISH && r == deflate.DONE && !e->done) {
		uint8_t trailer[8];
		store32le(trailer + 0, e->check);
		store32le(trailer + 4, (uint32_t) e->total);
		deflate.deflate_append(e->def, trailer, 8);
		e->done = 1;
		r = deflate.deflate(e->def, b, flush);
	}
	seterr(err, r);
	return r;
}

//
// The compressor writer
//

// A writer that encodes what is written to it as one gzip stream,
// forwarding the compressed bytes to another writer. The bytes of each
// write go to the deflator whole: it stages them internally, so the
// writer needs no input buffer of its own. Compressed output is staged
// in COMPRESSOR_OUT bytes.
const int COMPRESSOR_OUT = 4096;

typedef {
	writer.t *out;
	deflator_t *e;
	uint8_t outbuf[COMPRESSOR_OUT];
	bool finished; // the stream has finished
	int32_t status; // how it ended, once it has
} compressor_t;

// Returns a writer that encodes everything written to it as one gzip
// stream at a level, forwarding the compressed bytes to out. Writes
// return the byte count or -1 on error. Finish the stream with finish,
// which writes the trailer; writes after that fail. Free it with
// writer.free. The wrapped writer stays the caller's.
pub writer.t *compressor(writer.t *out, int32_t level) {
	if (!out) {
		return 0;
	}
	compressor_t *g = calloc!(1, (sizeof(compressor_t)));
	g->out = out;
	g->e = deflate_create(level);
	if (!g->e) {
		free(g);
		return 0;
	}
	return writer.new(g, compressor_write, compressor_free);
}

// Frees a compressor writer's state.
void compressor_free(void *ctx) {
	compressor_t *g = ctx;
	if (g) {
		deflate_free(g->e);
		free(g);
	}
}

// Finishes the compressor writer's stream, writing the trailer. On failure,
// such as a sticky stream error, sets err and leaves the stream ended.
// Finishing an already-finished writer writes nothing more.
pub void finish(writer.t *w, error.t *err) {
	if (!w) {
		panic("null argument");
	}
	compressor_t *g = w->data;
	if (!g->finished && !g->status) {
		g->finished = true;
		g->status = compressor_pump(g, 0, 0, deflate.FINISH);
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
int compressor_write(void *ctx, const uint8_t *data, size_t n) {
	compressor_t *g = ctx;
	if (!n) {
		return 0;
	}
	if (g->finished || g->status) {
		return -1;
	}
	int32_t st = compressor_pump(g, data, n, deflate.NONE);
	if (st < deflate.DONE) {
		g->status = st;
		return -1;
	}
	return (int) n;
}

// Run the deflator over n input bytes with a flush mode, writing every
// compressed byte to the wrapped writer as the output staging fills.
// Returns the deflator status.
int32_t compressor_pump(compressor_t *g, const uint8_t *data, size_t n, int32_t flush) {
	deflate.zbuf b;
	b.in = data;
	b.inlen = n;
	while (true) {
		b.out = g->outbuf;
		b.outlen = (size_t) COMPRESSOR_OUT;
		int32_t st = deflate(g->e, &b, flush, 0);
		write_all(g->out, g->outbuf, (size_t) COMPRESSOR_OUT - b.outlen);
		if (st == deflate.NEED_OUTPUT) {
			continue;
		}
		return st;
	}
}
