// LZW with GIF flavor.

#import bits
#import reader
#import writer

//
// Dict
//

const size_t MAX_DICT_LENGTH = 4096;

typedef {
	uint8_t len;
	uint8_t *word;
} word_t;

typedef {
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
	size_t largest_code = dictsize - 1; // because zero-based
	uint8_t n = 1;
	size_t max = 2;
	while (max < largest_code) {
		n++;
		max *= 2;
	}
	return n;
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
	enc->bw = bits.newwriter(out);
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
	uint8_t a = code / 256;
	uint8_t b = code % 256;
	uint8_t bb[16] = {};
	bits.getbits_msfirst(a, bb);
	bits.getbits_msfirst(b, &bb[8]);
	bits.write(enc->bw, &bb[16-w], w);
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

typedef {
	dict_t dict; // code dictionary
	bits.reader_t *br; // input bits reader
} dec_t;

pub void decompress(reader.t *in, size_t alphabet_size, writer.t *out) {
	dec_t dec = {};
	dec.dict = newdict(alphabet_size);
	dec.br = bits.newreader(in);
	
	uint8_t prev[4096] = {};
	size_t prevlen = 0;
	uint8_t curr[4096] = {};
	size_t currlen = 0;
	
	while (true) {
		size_t mi = dec_getcode(&dec);
		if (mi > dec.dict.size) {
			panic("got code %zu out of bounds [0, %zu)", mi, dec.dict.size);
		}

		if (mi == dec.dict.resetcode) {
			resetdict(&dec.dict);
			continue;
		}
		if (mi == dec.dict.endcode) {
			break;
		}

		// Emit the word for the code.
		if (mi == dec.dict.size) {
			// Because the decoder is one step behind the encoder,
			// it's possible that the next code is not in the dictionary yet.
			// This can happen only if the next code is for the word
			// prev + prev[0]:
			// 
			// abc | ?xyz
			// => abc? = ?xyz
			// => abc | abca
			if (prevlen == 0) {
				panic("invalid code");
			}
			memcpy(curr, prev, prevlen);
			currlen = prevlen;
			curr[currlen++] = curr[0];
		} else {
			word_t *e = &dec.dict.entries[mi];
			memcpy(curr, e->word, e->len);
			currlen = e->len;
		}

		int r = writer.write(out, curr, currlen);
		if (r < 0 || (size_t) r != currlen) {
			panic("write failed, r = %d\n", r);
		}

		// Add prev + curr[0] to the dictionary.
		if (prevlen > 0) {
			if (dec.dict.size == MAX_DICT_LENGTH) {
				resetdict(&dec.dict);
			}
			prev[prevlen++] = curr[0];
			addentry(&dec.dict, prev, prevlen);
		}

		// prev = curr
		memcpy(prev, curr, currlen);
		prevlen = currlen;
	}
}

size_t dec_getcode(dec_t *dec) {
	// The decompressor lags one step behind the compressor:
	// compressor writes code n while having updated the dict n-1 times, whereas
	// decompressor reads code n while having updated the dict n-2 times.
	uint8_t w = codewidth(dec->dict.size + 1);
	int code = bits.readn(dec->br, w);
	return (size_t) code;
}
