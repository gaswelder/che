// LZW with GIF flavor.

#import bits
#import reader
#import writer

//
// Dict
//

const size_t MAX_DICT_LENGTH = 4096;

pub typedef {
	uint8_t len;
	uint8_t *word;
} word_t;

pub typedef {
	size_t init_size;
	size_t size;
	word_t *entries;
	uint16_t resetcode; // code value for RESET
	uint16_t endcode; // code value for END
} dict_t;

// Creates a new dictionary with preallocated codes for [0..n),
// plus two special codes for RESET (n) and END (n+1).
dict_t newdict(size_t n) {
	dict_t dict = {};
	dict.entries = calloc!(MAX_DICT_LENGTH, sizeof(word_t));
	for (size_t i = 0; i < n; i++) {
		dict.entries[i].len = 1;
		dict.entries[i].word = calloc!(1, 1);
		dict.entries[i].word[0] = i;
	}
	dict.size = n;

	// Reserve codes for CLEAR and END.
	dict.size += 2;
	dict.init_size = dict.size;
	dict.resetcode = n;
	dict.endcode = n + 1;
	// fprintf(stderr, "reset = %u, end = %u\n", dict.resetcode, dict.endcode);
	return dict;
}

void freedict(dict_t dict) {
	for (size_t i = 0; i < dict.size; i++) {
		free(dict.entries[i].word);
	}
	free(dict.entries);
}

void resetdict(dict_t *dict) {
	for (size_t i = dict->init_size; i < dict->size; i++) {
		free(dict->entries[i].word);
	}
	dict->size = dict->init_size;
}

// Adds an entry to the dictionary.
void addentry(dict_t *dict, uint8_t *word, size_t wordlen) {
	if (dict->size == MAX_DICT_LENGTH) {
		panic("max dict length");
	}
	word_t *e = &dict->entries[dict->size++];
	e->len = wordlen;
	e->word = calloc!(wordlen, 1);
	memcpy(e->word, word, wordlen);
}

// Returns the smallest size in bits that fits all codes in the dictionary.
uint8_t codewidth(size_t dictsize) {
	size_t largest_code = dictsize - 1;
	uint8_t width = 1;
	size_t maxcode = 1;
	while (maxcode < largest_code) {
		width++;
		maxcode *= 2;
	}
	return width;
}


//
// Compressor
//

typedef {
	dict_t dict; // the code dictionary
	bits.writer_t *bw; // bits output writer
} enc_t;

// Creates a new instance of the compressor.
enc_t *init(size_t alphabet_size, writer.t *out) {
	enc_t *enc = calloc!(1, sizeof(enc_t));
	enc->dict = newdict(alphabet_size);
	enc->bw = bits.newwriter(out, bits.REVERSED);
	return enc;
}

pub void compress(reader.t *r, size_t alphabet_size, writer.t *out) {
	enc_t *enc = init(alphabet_size, out);

	peeker_t *p = calloc!(1, sizeof(peeker_t));
	p->reader = r;

	while (true) {
		// Max. possible word length is 4096 - alphabet_size,
		// so prefetching 4096 guarantees we have more than any word.
		// 4096 is also a nice chunk size for input-output.
		size_t q = peek(p, 4096);
		if (q == 0) {
			break;
		}

		size_t mi = match(enc, p->buf, q);
		size_t mlen = enc->dict.entries[mi].len;
		comp_emit(enc, mi);

		// If we have matched the peek fully, then the peek has reached EOF.
		if (mlen == q) {
			break;
		}
		if (enc->dict.size == MAX_DICT_LENGTH) {
			// Write the reset code before resetting the dictionary
			// to stay at the current code width.
			comp_emit(enc, enc->dict.resetcode);
			resetdict(&enc->dict);
		}
		if (enc->dict.size < MAX_DICT_LENGTH) {
			addentry(&enc->dict, p->buf, mlen + 1);
		}
		shift(p, mlen);
	}

	// Finalizes the compressed stream and frees the compressor.
	free(p);
	comp_emit(enc, enc->dict.endcode);
	bits.closewriter(enc->bw);
	freedict(enc->dict);
	free(enc);
}

// Finds the best word match for the input.
size_t match(enc_t *enc, uint8_t *input, size_t inputlen) {
	size_t mi = 0;
	size_t mlen = 0;
	bool ok = false;
	for (size_t i = 0; i < enc->dict.size; i++) {
		word_t *e = &enc->dict.entries[i];
		if (e->len <= mlen || e->len > inputlen) {
			continue;
		}
		if (memcmp(e->word, input, e->len) == 0) {
			mi = i;
			mlen = e->len;
			ok = true;
		}
	}
	if (!ok) {
		panic("failed to match");
	}
	return mi;
}

void comp_emit(enc_t *enc, uint16_t code) {
	uint8_t w = codewidth(enc->dict.size);
	writebits(enc->bw, code, w);
}

typedef {
	uint8_t *buf;
	size_t bufsize;
	size_t datalen;
	reader.t *reader;
} peeker_t;

// Reads from the underlying reader enough bytes so that there is
// n bytes present in the buffer.
// If the input doesn't have enough bytes, reads what's left.
// Returns the size of the buffer.
size_t peek(peeker_t *p, size_t n) {
	// Ensure the buffer is big enough.
	if (p->bufsize < n) {
		if (!p->bufsize) {
			p->bufsize = 1;
		}
		while (p->bufsize < n) {
			p->bufsize *= 2;
		}
		p->buf = realloc(p->buf, p->bufsize);
		if (!p->buf) {
			panic("realloc failed");
		}
	}

	// Ensure the buffer has n, unless EOF.
	if (p->datalen < n) {
		int q = reader.read(p->reader, &p->buf[p->datalen], n - p->datalen);
		if (q != EOF) {
			p->datalen += (size_t) q;
		}
	}

	return p->datalen;
}

// Removes n bytes from the buffer, shifting the remaining bytes
// to the start of the buffer.
void shift(peeker_t *p, size_t n) {
	if (n == 0 || n > p->datalen) {
		panic("shift: invalid n (=%zu)", n);
	}
	memmove(p->buf, &p->buf[n], p->datalen - n);
	p->datalen -= n;
}


//
// Decompressor
//

pub typedef {
	dict_t dict; // code dictionary
	bits.reader_t *br; // input bits reader
	uint8_t prev[4096];
	uint8_t curr[4096];
	size_t prevlen;
	size_t currlen;
} dec_t;

pub dec_t *newdecoder(reader.t *in, size_t alphabet_size) {
	dec_t *d = calloc!(1, sizeof(dec_t));
	d->dict = newdict(alphabet_size);
	d->br = bits.newreader(in, bits.REVERSED);
	return d;
}

pub void freedecoder(dec_t *d) {
	bits.closereader(d->br);
	freedict(d->dict);
	free(d);
}

// Decodes the entire stream from dec, writing decoded output into out.
pub void decodeinto(dec_t *dec, writer.t *out) {
	uint8_t buf[4096] = {};
	while (true) {
		int n = decode(dec, buf);
		if (n == 0) {
			break;
		}
		int r = writer.write(out, buf, (size_t) n);
		if (r < 0 || r != n) {
			panic("write failed, r = %d\n", r);
		}
	}
}

void writebits(bits.writer_t *w, uint16_t code, uint8_t n) {
	// for (uint8_t i = 0; i < n; i++) {
	// 	int s = n - i - 1;
	// 	int m = 1 << s;
	// 	uint8_t bit = (code & m) >> s;
	// 	bits.write1(w, bit);
	// }


	for (uint8_t i = 0; i < n; i++) {
		uint8_t bit = code % 2;
		code /= 2;
		bits.write1(w, bit);
	}
}

uint16_t readbits(bits.reader_t *r, uint8_t n) {
	// uint16_t val = 0;
	// for (uint8_t i = 0; i < n; i++) {
	// 	int c = bits.read1(r);
	// 	if (c < 0) panic("read failed");
	// 	val *= 2;
	// 	val += c;
	// }


	uint16_t val = 0;
	uint16_t amp = 1;
	for (uint8_t i = 0; i < n; i++) {
		int c = bits.read1(r);
		if (c < 0) panic("read failed");
		val += amp * c;
		amp *= 2;
	}


	// printf("--- %u bits val: %u\n", n, val);
	return val;
}

// Reads next code from the input stream and puts the decoded
// string into buf.
// buf must be of size 4096.
// Returns the length of the decoded string or zero on end of decoded data.
pub int decode(dec_t *dec, uint8_t *buf) {
	uint16_t code = 0;
	for (int i = 0; i < 2; i++) {
		// The decoder lags one step behind the encoder:
		// encoder writes code n while having updated the dict n-1 times,
		// decoder reads code n while having updated the dict n-2 times.
		uint8_t w = codewidth(dec->dict.size + 1);
		code = readbits(dec->br, w);
		if (code == dec->dict.resetcode) {
			resetdict(&dec->dict);
			continue;
		}
			break;
		}
	if (code > dec->dict.size) {
		panic("got code %u out of bounds [0, %zu)", code, dec->dict.size);
	}
	if (code == dec->dict.endcode) {
		return 0;
	}
	if (code == dec->dict.size) {
			// Because the decoder is one step behind the encoder,
			// it's possible that the next code is not in the dictionary yet.
			// This can happen only if the next code is for the word
			// prev + prev[0]:
			// 
			// abc | ?xyz
			// => abc? = ?xyz
			// => abc | abca
		if (dec->prevlen == 0) {
				panic("invalid code");
			}
		memcpy(dec->curr, dec->prev, dec->prevlen);
		dec->currlen = dec->prevlen;
		dec->curr[dec->currlen++] = dec->curr[0];
		} else {
		word_t *e = &dec->dict.entries[code];
		memcpy(dec->curr, e->word, e->len);
		dec->currlen = e->len;
		}

	int ret = (int) dec->currlen;
	memcpy(buf, dec->curr, dec->currlen);

		// Add prev + curr[0] to the dictionary.
	if (dec->prevlen > 0) {
		if (dec->dict.size == MAX_DICT_LENGTH) {
			resetdict(&dec->dict);
			}
		dec->prev[dec->prevlen++] = dec->curr[0];
		addentry(&dec->dict, dec->prev, dec->prevlen);
		}

		// prev = curr
	memcpy(dec->prev, dec->curr, dec->currlen);
	dec->prevlen = dec->currlen;

	return ret;
}
