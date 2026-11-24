#import reader
#import writer

const char *alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789+/";

pub typedef {
	writer.t *w;
	uint16_t buf;
	uint8_t buflen;
} encoder_t;

// Returns a base64 encoder writing to w.
pub encoder_t *newencoder(writer.t *w) {
	encoder_t *bw = calloc!(1, sizeof(encoder_t));
	bw->w = w;
	return bw;
}

// Encodes data.
pub void encode(encoder_t *bw, uint8_t *data, size_t len) {
	for (size_t i = 0; i < len; i++) {
		xput(bw, data[i]);
	}
}

void xput(encoder_t *bw, uint8_t c) {
	writer.t *w = bw->w;
	bw->buf = bw->buf << 8;
	bw->buf += c;
	bw->buflen += 8;
	while (bw->buflen >= 6) {
		uint8_t b6 = alpha[(bw->buf >> (bw->buflen-6))];
		writer.write(w, &b6, 1);
		bw->buflen -= 6;
		bw->buf &= (1 << bw->buflen) - 1;
	}
}

// Finalizes output and frees the encoder.
pub void closeencoder(encoder_t *bw) {
	writer.t *w = bw->w;
	switch (bw->buflen) {
		case 4: {
			uint8_t c = alpha[bw->buf << 2];
			writer.write(w, &c, 1);
			writer.write(w, (uint8_t *)"=", 1);
		}
		case 2: {
			uint8_t c = alpha[bw->buf << 4];
			writer.write(w, &c, 1);
			writer.write(w, (uint8_t *)"==", 2);
		}
	}
	free(bw);
}

pub typedef {
	reader.t *r;
	uint16_t buf;
	uint8_t buflen;
	bool ended;
} decoder_t;

// Returns a base64 decoder reading from r.
pub decoder_t *newdecoder(reader.t *r) {
	decoder_t *d = calloc!(1, sizeof(decoder_t));
	d->r = r;
	return d;
}

// Decodes up to bufsize bytes into buf.
pub int decode(decoder_t *d, uint8_t *buf, size_t bufsize) {
	size_t len = 0;
	for (size_t i = 0; i < bufsize; i++) {
		int c = xread(d);
		if (c == EOF) {
			break;
		}
		buf[len++] = (uint8_t) c;
	}
	return (int) len;
}

int xread(decoder_t *d) {
	if (d->ended) {
		return EOF;
	}
	while (d->buflen < 8) {
		uint8_t c = 0;
		int res = reader.read(d->r, &c, 1);
		if (res == EOF && d->buflen == 0) {
			d->ended = true;
			return EOF;
		}
		if (res != 1) {
			panic("read failed");
		}
		if ((char) c == '=') {
			if ((d->buflen != 4 && d->buflen != 2) || d->buf != 0) {
				panic("invalid base64 padding");
			}
			if (d->buflen == 4) {
				if (reader.read(d->r, &c, 1) != 1 || (char) c != '=') {
					panic("expected second padding '='");
				}
			}
			if (reader.read(d->r, &c, 1) != EOF) {
				panic("data after base64 padding");
			}
			d->ended = true;
			return EOF;
		}
		d->buf = (d->buf << 6) | lookup(c);
		d->buflen += 6;
	}

	d->buflen -= 8;
	uint8_t v8 = d->buf >> d->buflen;
	d->buf &= (1 << d->buflen) - 1;
	return v8;
}

pub void freedecoder(decoder_t *d) {
	free(d);
}

uint8_t lookup(uint8_t c) {
	for (uint8_t i = 0; i < 64; i++) {
		if ((uint8_t) alpha[i] == c) {
			return i;
		}
	}
	panic("invalid base64 character: %u", c);
}
