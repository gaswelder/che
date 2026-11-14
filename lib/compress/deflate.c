// DEFLATE encoder, RFC 1951.
// Ported from https://github.com/skeeto/tugz.

// Flush modes.
pub enum {
	NONE,
	SYNC,   // byte-align output with an empty stored block
	FULL,   // also forget history
	FINISH, // end the stream
}

// Results. Positive values ask for another call. DONE is -1, and errors
// are below it.
pub enum {
	DONE = -1,
	NEED_INPUT = 1,
	NEED_OUTPUT = 2,
	ETRUNC = -2,
	EDATA = -3,
	ENOTGZ = -4,
	EHEADER = -5,
	ECHECK = -6,
	ELENGTH = -7,
	EUSAGE = -8,
}

// Streaming buffers, advanced as input is consumed and output produced.
pub typedef {
	const uint8_t *in;
	size_t inlen;
	uint8_t *out;
	size_t outlen;
} zbuf;

// Explicit little-endian accesses.
uint64_t load64le(const uint8_t *p) {
	return (uint64_t) p[0] | ((uint64_t) p[1] << 8) | ((uint64_t) p[2] << 16) | ((uint64_t) p[3] << 24) | ((uint64_t) p[4] << 32) | ((uint64_t) p[5] << 40) | ((uint64_t) p[6] << 48) | ((uint64_t) p[7] << 56);
}

void store64le(uint8_t *p, uint64_t v) {
	p[0] = (uint8_t) v;
	p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
	p[4] = (uint8_t)(v >> 32);
	p[5] = (uint8_t)(v >> 40);
	p[6] = (uint8_t)(v >> 48);
	p[7] = (uint8_t)(v >> 56);
}

uint32_t load32le(const uint8_t *p) {
	return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24);
}

const int DEF_WSIZE = 32768;
const int DEF_WMASK = 32767;
const int MIN_MATCH = 3;
const int MAX_MATCH = 258;
const int HASH_LEN = 4;
const int HASH_BITS = 16;
const int HASH_SIZE = 65536;
const int D3_MAX = 4096;
const int LOOKAHEAD = 290;
const int WIN_CAP = 1081344; // (1 << 20) + DEF_WSIZE
const int TOK_CAP = 65535;
const int TOK_LIMIT = 65532; // TOK_CAP - 3
const int DEF_STAGE_NEED = 525314; // TOK_CAP*6 + 2*(65535+5) + 1024
const int DEF_STAGE = 590850; // DEF_STAGE_NEED + (1 << 16)
const int NLIT = 286;
const int NDIST = 30;
const int NCL = 19;
const int NOBS = 10;
const int OBS_BATCH = 512;
const int MIN_BLOCK = 10000;
const int FORGET_REHASH = 1024;
const int BLOCK_CAP = 65532;
const int NARROW_LEVEL = 5;
const uint32_t STAMP_STEP = 2097152; // 1 << 21
const uint32_t STAMP_LAST = 0xffe00000;

pub typedef {
	uint16_t litlen;
	uint16_t dist; // zero for literals
} token_t;

pub typedef {
	int32_t depth; // hash chain search depth
	int32_t nice; // stop searching at a match this long
	int32_t lazy; // lazy evaluation steps (0 for greedy)
	int32_t good; // skip lazy evaluation, and shorten search, past this
	int32_t depth3; // 3-byte chain search depth
	int32_t insert; // insert positions inside matches up to this long
	int32_t bucket; // entries per bucket of the bucket finder, or 0 (chains)
} deflate_level_t;

const deflate_level_t deflate_levels[10] = {
	// depth nice lazy good depth3 insert bucket
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 16, 1 },
	{ 0, 32, 0, 0, 0, 16, 2 },
	{ 8, 32, 0, 8, 8, 32, 0 },
	{ 8, 32, 1, 8, 8, 258, 0 },
	{ 16, 64, 1, 16, 16, 258, 0 },
	{ 22, 128, 1, 128, 32, 258, 0 },
	{ 64, 128, 2, 32, 32, 258, 0 },
	{ 256, 258, 2, 64, 64, 258, 0 },
	{ 1024, 258, 2, 258, 256, 258, 0 },
};

pub typedef {
	uint8_t len[288];
	uint16_t code[288];
} htree_t;

// A dynamic block's codes and their run-length encoded description.
pub typedef {
	htree_t lt, dt, cl;
	int32_t hlit, hdist, hclen;
	int32_t nrle;
	uint8_t rsym[NLIT + NDIST];
	uint8_t rextra[NLIT + NDIST];
} dynblock_t;

pub typedef {
	deflate_level_t lvl;

	uint8_t *obuf; // staged output: [ooff, olen) is pending
	size_t olen;
	size_t ooff;
	int32_t blkready; // a block is complete and should be emitted
	int32_t flushing; // flush due once a call's input is in, or NONE
	int32_t flushed; // flushing's output is staged, awaiting drain
	int32_t finq; // a FINISH is due after flushing and those queued
	uint64_t nqueued; // SYNC/FULL flushes due after flushing, alternating

	uint8_t *win;
	size_t win_len;
	uint64_t base; // stream offset of win[0]
	size_t pos; // next position to parse
	size_t ins; // next position to insert into hash chains
	int32_t sampled;
	int32_t use3;

	uint32_t *head; // or the buckets (see parse_bucket_with)
	uint32_t *prev;
	uint32_t *head3; // follows head
	uint32_t *prev3;
	uint32_t *seen; // sample's bitmap of hashes, zero between samples
	uint32_t stamp; // added to hash table entries; see forget

	token_t *toks;
	size_t ntok;

	uint64_t blk_start;
	uint64_t blk_len;
	uint64_t pend_start;
	size_t pend_len;

	// Block splitting statistics: coarse symbol categories observed in
	// the current block, and in the latest batch of observations.
	uint32_t obs[NOBS];
	uint32_t newobs[NOBS];
	uint32_t nobs;
	uint32_t nnewobs;

	uint32_t lit_freq[NLIT];
	uint32_t dist_freq[NDIST];

	uint64_t bitbuf;
	int32_t bitcnt;

	htree_t fixlit;
	htree_t fixdist;
	dynblock_t dyn;

	uint8_t lcode[MAX_MATCH + 1]; // length -> length code index
	uint8_t dcode[512]; // see dcode_of
	uint32_t lenbits[MAX_MATCH + 1]; // block's length codes, see emit_tokens
	int32_t narrow; // links chains by distances
} deflator_t;

const uint16_t def_len_base[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
const uint8_t def_len_extra[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
const uint16_t def_dist_base[30] = {
	    1,     2,     3,     4,     5,     7,     9,    13,
	   17,    25,    33,    49,    65,    97,   129,   193,
	  257,   385,   513,   769,  1025,  1537,  2049,  3073,
	 4097,  6145,  8193, 12289, 16385, 24577
};
const uint8_t def_dist_extra[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
const uint8_t def_cl_order[NCL] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

int32_t count_leading_zeros(uint32_t x) {
	int32_t n = 0;
	if (!x) {
		return 32;
	}
	while (!(x & 0x80000000)) {
		x = x << 1;
		n++;
	}
	return n;
}

int32_t count_trailing_zeros64(uint64_t x) {
	int32_t n = 0;
	while (!(x & 1)) {
		x = x >> 1;
		n++;
	}
	return n;
}

// Length code index (0..28) for a match length in 3..258.
int32_t length_code(int32_t len) {
	uint32_t x = (uint32_t) len - 3;
	if (x < 8) {
		return (int32_t) x;
	}
	if (len == 258) {
		return 28;
	}
	int32_t b = 31 - count_leading_zeros(x);
	return 4 * (b - 1) + (int32_t)((x >> (b - 2)) & 3);
}

// Distance code (0..29) for a distance in 1..32768.
int32_t dist_code(int32_t dist) {
	uint32_t x = (uint32_t) dist - 1;
	if (x < 4) {
		return (int32_t) x;
	}
	int32_t b = 31 - count_leading_zeros(x);
	return 2 * b + (int32_t)((x >> (b - 1)) & 1);
}

int32_t lcode_of(deflator_t *d, int32_t len) {
	return d->lcode[len];
}

int32_t dcode_of(deflator_t *d, int32_t dist) {
	if (dist <= 256) {
		return d->dcode[dist - 1];
	}
	return d->dcode[256 + (dist - 1 >> 7)];
}

uint32_t bit_reverse(uint32_t v, int32_t n) {
	uint32_t r = 0;
	for (int32_t i = 0; i < n; i++) {
		r = (r << 1) | (v & 1);
		v = v >> 1;
	}
	return r;
}

// Assign canonical codes from lengths already stored in t.
void huff_codes(htree_t *t, int32_t n) {
	uint32_t count[16] = { 0 };
	for (int32_t i = 0; i < n; i++) {
		count[t->len[i]]++;
	}
	count[0] = 0;
	uint32_t next[16];
	uint32_t code = 0;
	for (int32_t b = 1; b < 16; b++) {
		code = code + count[b - 1] << 1;
		next[b] = code;
	}
	for (int32_t i = 0; i < n; i++) {
		int32_t l = t->len[i];
		if (l) {
			t->code[i] = (uint16_t) bit_reverse(next[l]++, l);
		} else {
			t->code[i] = 0;
		}
	}
}

// Moffat & Katajainen in-place minimum redundancy code. On input, w holds
// n weights in ascending order. On output it holds code lengths, which
// are therefore in descending order.
void min_redundancy(uint32_t *w, int32_t n) {
	if (n == 1) {
		w[0] = 1;
		return;
	}

	w[0] += w[1];
	int32_t root = 0;
	int32_t leaf = 2;
	for (int32_t next = 1; next < n - 1; next++) {
		if (leaf >= n || w[root] < w[leaf]) {
			w[next] = w[root];
			w[root++] = (uint32_t) next;
		} else {
			w[next] = w[leaf++];
		}
		if (leaf >= n || root < next && w[root] < w[leaf]) {
			w[next] += w[root];
			w[root++] = (uint32_t) next;
		} else {
			w[next] += w[leaf++];
		}
	}

	w[n - 2] = 0;
	for (int32_t next = n - 3; next >= 0; next--) {
		w[next] = w[w[next]] + 1;
	}

	int32_t avail = 1;
	int32_t used = 0;
	uint32_t depth = 0;
	root = n - 2;
	int32_t next = n - 1;
	while (avail > 0) {
		for (; root >= 0 && w[root] == depth; root--) {
			used++;
		}
		for (; avail > used; avail--) {
			w[next--] = depth;
		}
		avail = 2 * used;
		used = 0;
		depth++;
	}
}

uint32_t huff_w(const uint32_t *freq, uint16_t s) {
	if (freq[s]) {
		return freq[s];
	}
	return 1;
}

// Build a complete, length-limited Huffman code. At least two symbols
// always get codes, as zlib does, so every code is complete.
void huff_build(htree_t *t, const uint32_t *freq, int32_t n, int32_t maxlen) {
	uint16_t sa[288];
	uint16_t sb[288];
	uint32_t w[288];
	int32_t m = 0;

	for (int32_t i = 0; i < n; i++) {
		t->len[i] = 0;
		if (freq[i]) {
			sa[m++] = (uint16_t) i;
		}
	}
	for (int32_t i = 0; m < 2; i++) {
		if (!freq[i]) {
			sa[m++] = (uint16_t) i;
		}
	}
	if (sa[0] > sa[1]) {
		uint16_t s = sa[0]; // a filler below the one used symbol
		sa[0] = sa[1];
		sa[1] = s;
	}

	// Sort by weight (unused fillers weigh 1), ties by symbol: symbols
	// start in order, and an LSD radix sort on the weight's bytes, being
	// stable, keeps ties so.
	uint32_t maxw = 0;
	for (int32_t i = 0; i < m; i++) {
		uint32_t v = huff_w(freq, sa[i]);
		maxw = max(maxw, v);
	}
	uint16_t *syms = sa;
	uint16_t *tmp = sb;
	for (int32_t shift = 0; shift < 32 && maxw >> shift; shift += 8) {
		uint16_t cnt[256] = { 0 };
		for (int32_t i = 0; i < m; i++) {
			cnt[(huff_w(freq, syms[i]) >> shift) & 255]++;
		}
		uint16_t sum = 0;
		for (int32_t i = 0; i < 256; i++) {
			uint16_t c = cnt[i];
			cnt[i] = sum;
			sum = sum + c;
		}
		for (int32_t i = 0; i < m; i++) {
			tmp[cnt[(huff_w(freq, syms[i]) >> shift) & 255]++] = syms[i];
		}
		uint16_t *x = syms;
		syms = tmp;
		tmp = x;
	}
	for (int32_t i = 0; i < m; i++) {
		w[i] = huff_w(freq, syms[i]);
	}

	min_redundancy(w, m);

	// Clamp to maxlen, then restore the Kraft equality by lengthening
	// shorter codes until the overflow is absorbed.
	uint32_t count[16] = { 0 };
	for (int32_t i = 0; i < m; i++) {
		count[min(w[i], (uint32_t) maxlen)]++;
	}
	uint32_t total = 0;
	for (int32_t b = 1; b <= maxlen; b++) {
		total += count[b] << (maxlen - b);
	}
	while (total != (uint32_t)((uint32_t) 1 << maxlen)) {
		count[maxlen]--;
		for (int32_t b = maxlen - 1; b > 0; b--) {
			if (count[b]) {
				count[b]--;
				count[b + 1] += 2;
				break;
			}
		}
		total--;
	}

	// Lowest weights first, so assign longest lengths first
	int32_t k = 0;
	for (int32_t b = maxlen; b > 0; b--) {
		for (uint32_t j = 0; j < count[b]; j++) {
			t->len[syms[k++]] = (uint8_t) b;
		}
	}
	huff_codes(t, n);
}

// Append n bits, n <= 32. Whole bytes spill into the staging buffer
// 8 bytes at a time once 32 or more bits accumulate.
void bw_put(deflator_t *d, uint64_t v, int32_t n) {
	d->bitbuf |= v << d->bitcnt;
	d->bitcnt += n;
	if (d->bitcnt >= 32) {
		store64le(d->obuf + d->olen, d->bitbuf);
		d->olen += d->bitcnt >> 3;
		d->bitbuf = d->bitbuf >> (d->bitcnt & ~7);
		d->bitcnt &= 7;
	}
}

// Pad to a byte boundary and spill all bits.
void bw_align(deflator_t *d) {
	d->bitcnt = (d->bitcnt + 7) & ~7;
	while (d->bitcnt) {
		d->obuf[d->olen++] = (uint8_t) d->bitbuf;
		d->bitbuf = d->bitbuf >> 8;
		d->bitcnt -= 8;
	}
}

// Append whole bytes, such as a container header or trailer, at a byte
// boundary.
void bytes(deflator_t *d, const uint8_t *p, size_t len) {
	memcpy(d->obuf + d->olen, p, len);
	d->olen += len;
}

int32_t cl_extra_bits(int32_t s) {
	switch (s) {
		case 16: { return 2; }
		case 17: { return 3; }
		case 18: { return 7; }
	}
	return 0;
}

// Run-length encode code lengths into code length code symbols.
int32_t rle_lengths(const uint8_t *lens, int32_t n, uint8_t *sym, uint8_t *extra) {
	int32_t m = 0;
	for (int32_t i = 0; i < n;) {
		int32_t l = lens[i];
		int32_t j = i + 1;
		for (; j < n && (int32_t) lens[j] == l; j++) {
		}
		int32_t run = j - i;
		i = j;

		if (!l) {
			for (; run >= 11; m++) {
				int32_t r = min(run, 138);
				sym[m] = 18;
				extra[m] = (uint8_t)(r - 11);
				run -= r;
			}
			for (; run >= 3; m++) {
				int32_t r = min(run, 10);
				sym[m] = 17;
				extra[m] = (uint8_t)(r - 3);
				run -= r;
			}
		} else {
			sym[m] = (uint8_t) l;
			extra[m++] = 0;
			run--;
			for (; run >= 3; m++) {
				int32_t r = min(run, 6);
				sym[m] = 16;
				extra[m] = (uint8_t)(r - 3);
				run -= r;
			}
		}
		while (run > 0) {
			sym[m] = (uint8_t) l;
			extra[m] = 0;
			run--;
			m++;
		}
	}
	return m;
}

void build_dyn(deflator_t *d, dynblock_t *b) {
	uint32_t lf[NLIT];
	for (int32_t i = 0; i < NLIT; i++) {
		lf[i] = d->lit_freq[i];
	}
	lf[256] = 1;
	huff_build(&b->lt, lf, NLIT, 15);
	huff_build(&b->dt, d->dist_freq, NDIST, 15);

	int32_t hlit = NLIT;
	for (; hlit > 257 && !b->lt.len[hlit - 1]; hlit--) {
	}
	int32_t hdist = NDIST;
	for (; hdist > 1 && !b->dt.len[hdist - 1]; hdist--) {
	}
	b->hlit = hlit;
	b->hdist = hdist;

	uint8_t lens[NLIT + NDIST];
	memcpy(lens, b->lt.len, hlit);
	memcpy(lens + hlit, b->dt.len, hdist);
	b->nrle = rle_lengths(lens, hlit + hdist, b->rsym, b->rextra);

	uint32_t clf[NCL] = { 0 };
	for (int32_t i = 0; i < b->nrle; i++) {
		clf[b->rsym[i]]++;
	}
	huff_build(&b->cl, clf, NCL, 7);

	int32_t hclen = NCL;
	for (; hclen > 4 && !b->cl.len[def_cl_order[hclen - 1]]; hclen--) {
	}
	b->hclen = hclen;
}

// Size of the block's symbols under the given codes, from frequencies.
uint64_t cost_tokens(deflator_t *d, htree_t *lt, htree_t *dt) {
	uint64_t bits = lt->len[256];
	for (int32_t i = 0; i < 256; i++) {
		bits += (uint64_t) d->lit_freq[i] * (uint64_t) lt->len[i];
	}
	for (int32_t i = 0; i < 29; i++) {
		bits += (uint64_t) d->lit_freq[257 + i] * (uint64_t)(lt->len[257 + i] + def_len_extra[i]);
	}
	for (int32_t i = 0; i < NDIST; i++) {
		bits += (uint64_t) d->dist_freq[i] * (uint64_t)(dt->len[i] + def_dist_extra[i]);
	}
	return bits;
}

uint64_t cost_dyn(deflator_t *d, dynblock_t *b) {
	uint64_t bits = 3 + 5 + 5 + 4 + 3 * (uint64_t) b->hclen;
	for (int32_t i = 0; i < b->nrle; i++) {
		bits += (uint64_t) b->cl.len[b->rsym[i]] + (uint64_t) cl_extra_bits(b->rsym[i]);
	}
	return bits + cost_tokens(d, &b->lt, &b->dt);
}

uint64_t cost_stored(deflator_t *d) {
	uint64_t n = d->blk_len;
	uint64_t nchunks = (n + 65534) / 65535;
	uint64_t pad = (8 - ((uint64_t) d->bitcnt + 3) % 8) % 8;
	return 3 + pad + 32 + 8 * n + (nchunks - 1) * 40;
}

void emit_stored_chunk(deflator_t *d, size_t start, size_t len, int32_t final) {
	bw_put(d, (uint64_t) final, 1);
	bw_put(d, 0, 2);
	bw_align(d);
	bw_put(d, (uint64_t)((uint32_t) len) | ((uint64_t)(~(uint32_t)(len) & 0xffff) << 16), 32);
	bw_align(d);
	bytes(d, d->win + start, len);
}

void flush_pending(deflator_t *d, int32_t final) {
	size_t start = (size_t)(d->pend_start - d->base);
	size_t len = d->pend_len;
	d->pend_len = 0;
	if (!len && final) {
		emit_stored_chunk(d, 0, 0, 1);
	}
	for (size_t off = 0; off < len;) {
		size_t n = min(len - off, 65535);
		emit_stored_chunk(d, start + off, n, final && off + n == len);
		off += n;
	}
}

void emit_stored(deflator_t *d, int32_t final) {
	uint64_t start = d->blk_start;
	size_t len = (size_t) d->blk_len;
	if (d->pend_len) {
		if (d->pend_start + (uint64_t) d->pend_len == start) {
			start = d->pend_start;
			len += d->pend_len;
			d->pend_len = 0;
		} else {
			flush_pending(d, 0);
		}
	}

	// Hold back the final partial chunk so that it may merge with the
	// next stored block.
	size_t off = 0;
	for (; len - off > 65535; off += 65535) {
		emit_stored_chunk(d, (size_t)(start - d->base) + off, 65535, 0);
	}
	d->pend_start = start + (uint64_t) off;
	d->pend_len = len - off;
	if (final) {
		flush_pending(d, 1);
	}
}

// Emit the block's tokens.
void emit_tokens(deflator_t *d, htree_t *lt, htree_t *dt) {
	uint64_t bb = d->bitbuf;
	int32_t bc = d->bitcnt;
	uint8_t *out = d->obuf + d->olen;
	token_t *toks = d->toks;
	size_t ntok = d->ntok;

	// Each length's code and extra bits under this block's code (at most
	// 15+5 bits) above the low 8 bits, which hold their count
	for (int32_t len = MIN_MATCH; len <= MAX_MATCH; len++) {
		int32_t lc = lcode_of(d, len);
		int32_t ln = lt->len[257 + lc];
		uint32_t lx = (uint32_t)(len - (int32_t) def_len_base[lc]);
		uint32_t v = (uint32_t) lt->code[257 + lc] | (lx << ln);
		d->lenbits[len] = (v << 8) | (uint32_t)(ln + (int32_t) def_len_extra[lc]);
	}

	// Spill first, as bw_put may have left up to 31 bits
	store64le(out, bb);
	out += bc >> 3;
	bb = bb >> (bc & ~7);
	bc &= 7;

	for (size_t i = 0; i < ntok;) {
		token_t t = toks[i++];
		if (!t.dist) {
			uint32_t c = t.litlen;
			bb |= (uint64_t) lt->code[c] << bc;
			bc += lt->len[c];
			if (i < ntok && !toks[i].dist) {
				c = toks[i].litlen;
				bb |= (uint64_t) lt->code[c] << bc;
				bc += lt->len[c];
				i++;
				if (i < ntok && !toks[i].dist) {
					c = toks[i].litlen;
					bb |= (uint64_t) lt->code[c] << bc;
					bc += lt->len[c];
					i++;
				}
			}
		} else {
			// The length's bits from the table, then the distance's code
			// plus extra bits: at most 15+13 bits
			uint32_t le = d->lenbits[t.litlen];
			int32_t dc = dcode_of(d, (int32_t) t.dist);
			uint64_t dx = (uint32_t)((int32_t) t.dist - (int32_t) def_dist_base[dc]);
			int32_t dn = dt->len[dc];
			bb |= (uint64_t)(le >> 8) << bc;
			bc += (int32_t)(le & 0xff);
			bb |= ((uint64_t) dt->code[dc] | (dx << dn)) << bc;
			bc += dn + (int32_t) def_dist_extra[dc];
		}
		store64le(out, bb);
		out += bc >> 3;
		bb = bb >> (bc & ~7);
		bc &= 7;
	}

	d->bitbuf = bb;
	d->bitcnt = bc;
	d->olen = out - d->obuf;
	bw_put(d, lt->code[256], lt->len[256]);
}

void emit_dynamic(deflator_t *d, dynblock_t *b) {
	bw_put(d, 2, 2);
	bw_put(d, (uint64_t)((uint32_t)(b->hlit - 257)), 5);
	bw_put(d, (uint64_t)((uint32_t)(b->hdist - 1)), 5);
	bw_put(d, (uint64_t)((uint32_t)(b->hclen - 4)), 4);
	for (int32_t i = 0; i < b->hclen; i++) {
		bw_put(d, b->cl.len[def_cl_order[i]], 3);
	}
	for (int32_t i = 0; i < b->nrle; i++) {
		int32_t s = b->rsym[i];
		bw_put(d, b->cl.code[s], b->cl.len[s]);
		bw_put(d, b->rextra[i], cl_extra_bits(s));
	}
	emit_tokens(d, &b->lt, &b->dt);
}

// Empty the token buffer and its statistics for the next block.
void clear_block(deflator_t *d) {
	d->ntok = 0;
	d->blk_len = 0;
	memset(d->lit_freq, 0, (sizeof(d->lit_freq)));
	memset(d->dist_freq, 0, (sizeof(d->dist_freq)));
	memset(d->obs, 0, (sizeof(d->obs)));
	memset(d->newobs, 0, (sizeof(d->newobs)));
	d->nobs = 0;
	d->nnewobs = 0;
}

void flush_block(deflator_t *d, int32_t final) {
	if (!d->ntok) {
		if (final) {
			if (d->pend_len) {
				flush_pending(d, 1);
			} else {
				bw_put(d, 1, 1);
				bw_put(d, 1, 2);
				bw_put(d, d->fixlit.code[256], d->fixlit.len[256]);
			}
		}
		return;
	}

	dynblock_t *b = &d->dyn;
	build_dyn(d, b);

	uint64_t cd = cost_dyn(d, b);
	uint64_t cf = 3 + cost_tokens(d, &d->fixlit, &d->fixdist);
	// A block ends before a slide would discard its data, but check, as
	// storing reads the window.
	int32_t can_store = d->blk_start >= d->base && d->blk_start - d->base + d->blk_len <= (uint64_t) d->win_len;
	uint64_t cs = (uint64_t) -1;
	if (can_store) {
		cs = cost_stored(d);
	}

	if (cs <= cd && cs <= cf) {
		emit_stored(d, final);
	} else {
		if (d->pend_len) {
			flush_pending(d, 0);
		}
		bw_put(d, (uint64_t) final, 1);
		if (cf <= cd) {
			bw_put(d, 1, 2);
			emit_tokens(d, &d->fixlit, &d->fixdist);
		} else {
			emit_dynamic(d, b);
		}
	}

	clear_block(d);
}

// Decide whether the latest batch of observations differs enough from
// the block so far that a new block should start.
int32_t should_split(deflator_t *d) {
	int32_t split = 0;
	if (d->nobs) {
		uint64_t delta = 0;
		for (int32_t i = 0; i < NOBS; i++) {
			uint64_t expect = (uint64_t) d->obs[i] * (uint64_t) d->nnewobs;
			uint64_t actual = (uint64_t) d->newobs[i] * (uint64_t) d->nobs;
			if (actual > expect) {
				delta += actual - expect;
			} else {
				delta += expect - actual;
			}
		}
		uint64_t total = (uint64_t) d->nobs + (uint64_t) d->nnewobs;
		uint64_t cutoff = (uint64_t) d->nnewobs * 200 / 512 * (uint64_t) d->nobs;
		if (d->blk_len < (uint64_t) MIN_BLOCK && total < 8192) {
			cutoff += cutoff * (8192 - total) / 8192;
		}
		if (d->blk_len >= (uint64_t) MIN_BLOCK && delta + d->blk_len / 4096 * (uint64_t) d->nobs >= cutoff) {
			split = 1;
		}
	}
	for (int32_t i = 0; i < NOBS; i++) {
		d->obs[i] += d->newobs[i];
		d->newobs[i] = 0;
	}
	d->nobs += d->nnewobs;
	d->nnewobs = 0;
	return split;
}

void tok_end(deflator_t *d) {
	d->nnewobs++;
	if (!d->blkready && (d->ntok >= (size_t) TOK_LIMIT || d->nnewobs == (uint32_t) OBS_BATCH && should_split(d))) {
		d->blkready = 1;
	}
}

void tok_lit(deflator_t *d, size_t p) {
	if (!d->ntok) {
		d->blk_start = d->base + (uint64_t) p;
	}
	uint8_t c = d->win[p];
	d->blk_len++;
	token_t t;
	t.litlen = c;
	t.dist = 0;
	d->toks[d->ntok++] = t;
	d->lit_freq[c]++;
	d->newobs[(int32_t)(((c >> 5) & 6) | (c & 1))]++;
	tok_end(d);
}

void tok_match(deflator_t *d, size_t p, int32_t len, int32_t dist) {
	if (!d->ntok) {
		d->blk_start = d->base + (uint64_t) p;
	}
	d->blk_len += (uint64_t) len;
	token_t t;
	t.litlen = (uint16_t) len;
	t.dist = (uint16_t) dist;
	d->toks[d->ntok++] = t;
	d->lit_freq[257 + lcode_of(d, len)]++;
	d->dist_freq[dcode_of(d, dist)]++;
	int32_t oi = 8;
	if (len >= 9) {
		oi = 9;
	}
	d->newobs[oi]++;
	tok_end(d);
}

// The match finder's state, which parse keeps in a local.
typedef {
	uint8_t *win;
	uint32_t *head;
	uint32_t *prev;
	uint32_t *head3;
	uint32_t *prev3;
	size_t win_len;
	size_t lim; // positions below have HASH_LEN bytes to hash
	size_t ins; // next position to insert into hash chains
	uint32_t h; // hashes of ins, when below lim, computed ahead
	uint32_t h3;
	uint32_t base; // low bits of the stream offset of win[0]
	uint32_t stamp;
	int32_t use3;
	deflate_level_t lvl;
	int32_t narrow;
} finder_t;

// Index into prev tables for window position p.
uint32_t chain_slot(finder_t *f, size_t p) {
	return (f->base + (uint32_t) p) & (uint32_t) DEF_WMASK;
}

// Index into prev tables for a hash table entry's position. Stamps are
// multiples of the window size, so they drop out.
uint32_t entry_slot(finder_t *f, uint32_t entry) {
	return (f->base + entry - 1) & (uint32_t) DEF_WMASK;
}

const uint32_t LINK_NONE = 65535;

// A chain link as the levels from NARROW_LEVEL on store it, in the first
// half of the link tables viewed as uint16_t.
uint16_t chain_link(uint32_t entry, uint32_t old) {
	return (uint16_t) min(entry - old, LINK_NONE);
}

// Little-endian, so hashes (and therefore output) match across hosts.
uint32_t load32(const uint8_t *p) {
	return load32le(p);
}

uint32_t hash4(uint32_t v) {
	return v * (uint32_t) 2654435761 >> (uint32_t)(32 - HASH_BITS);
}

uint32_t hash3(uint32_t v) {
	return (v & 0xffffff) * (uint32_t) 2654435761 >> (uint32_t)(32 - HASH_BITS);
}

// Hash position x ahead of its insertion.
void hash_ahead(finder_t *f, size_t x) {
	if (x < f->lim) {
		uint32_t v = load32(f->win + x);
		f->h = hash4(v);
		if (f->use3) {
			f->h3 = hash3(v);
		}
	}
}

typedef {
	uint32_t c4; // the chains' previous heads
	uint32_t c3; // (zero when not using 3-byte chains)
} heads_t;

// Insert ins, which must be below lim, and hash the position after it.
heads_t hash_insert(finder_t *f) {
	size_t x = f->ins++;
	uint32_t h = f->h;
	uint32_t h3 = f->h3;
	hash_ahead(f, x + 1);
	uint32_t slot = chain_slot(f, x);
	uint32_t entry = (uint32_t) x + 1 + f->stamp;
	heads_t r;
	r.c4 = f->head[h];
	r.c3 = 0;
	uint16_t *p16 = (uint16_t *) f->prev;
	if (f->narrow) {
		p16[slot] = chain_link(entry, r.c4);
	} else {
		f->prev[slot] = r.c4;
	}
	f->head[h] = entry;
	if (f->use3) {
		r.c3 = f->head3[h3];
		uint16_t *p316 = (uint16_t *) f->prev3;
		if (f->narrow) {
			p316[slot] = chain_link(entry, r.c3);
		} else {
			f->prev3[slot] = r.c3;
		}
		f->head3[h3] = entry;
	}
	return r;
}

// Insert all positions through q.
void ensure_insert(finder_t *f, size_t q) {
	while (f->ins <= q && f->ins < f->lim) {
		hash_insert(f);
	}
	f->ins = max(f->ins, q + 1);
}

int32_t match_len(const uint8_t *a, const uint8_t *b, int32_t max) {
	int32_t len = 0;
	for (; len + 8 <= max; len += 8) {
		uint64_t x = load64le(a + len) ^ load64le(b + len);
		if (x) {
			return len + (count_trailing_zeros64(x) >> 3);
		}
	}
	for (; len < max && a[len] == b[len]; len++) {
	}
	return len;
}

typedef {
	int32_t len;
	int32_t dist;
} match_t;

// find_match for distance links.
match_t find_match16(finder_t *f, size_t p, uint32_t cand, uint32_t cand3, int32_t depth, int32_t best) {
	uint8_t *win = f->win;
	uint16_t *prev = (uint16_t *) f->prev;
	int32_t maxlen = (int32_t) min(f->win_len - p, (size_t) MAX_MATCH);
	int32_t least = best;
	size_t best_pos = 0;
	uint32_t pb = f->base + (uint32_t) p;
	uint32_t kb = f->base - 1 - f->stamp; // an entry's offset, less it
	uint32_t cb = cand + kb;
	uint32_t maxd = (uint32_t) min(p, (size_t) DEF_WSIZE);
	if (best >= maxlen) {
		cb = pb - LINK_NONE; // nothing longer fits
	}
	uint32_t p4 = 0;
	if (pb - cb <= maxd) {
		p4 = load32(win + p);
	}

	for (; best < MIN_MATCH && pb - cb <= maxd && depth > 0; depth--) {
		size_t c = p - (size_t)(pb - cb);
		cb -= (uint32_t) prev[cb & (uint32_t) DEF_WMASK];
		uint32_t x = load32(win + c) ^ p4;
		if (!(x & 0xffffff)) {
			if (x) {
				best = 3;
			} else {
				best = 4 + match_len(win + c + 4, win + p + 4, maxlen - 4);
			}
			best_pos = c;
			if (best >= f->lvl.nice || best == maxlen) {
				cb = pb - LINK_NONE;
				break;
			}
			if (best >= f->lvl.good && depth > 8) {
				depth = 8;
			}
		}
	}

	for (; pb - cb <= maxd && depth > 0; depth--) {
		size_t c = p - (size_t)(pb - cb);
		cb -= (uint32_t) prev[cb & (uint32_t) DEF_WMASK];
		if (load32(win + c + best - 3) == load32(win + p + best - 3) && load32(win + c) == p4) {
			int32_t len = 4 + match_len(win + c + 4, win + p + 4, maxlen - 4);
			if (len > best) {
				best = len;
				best_pos = c;
				if (best >= f->lvl.nice || best == maxlen) {
					break;
				}
				if (best >= f->lvl.good && depth > 8) {
					depth = 8;
				}
			}
		}
	}

	uint16_t *prev3 = (uint16_t *) f->prev3;
	int32_t depth3 = 0;
	if (best < MIN_MATCH) {
		depth3 = f->lvl.depth3;
	}
	uint32_t cb3 = cand3 + kb;
	uint32_t lim3 = (uint32_t) min((int32_t) maxd, (int32_t) D3_MAX);
	for (; pb - cb3 <= lim3 && depth3 > 0; depth3--) {
		size_t c = p - (size_t)(pb - cb3);
		if (win[c] == win[p] && win[c + 1] == win[p + 1] && win[c + 2] == win[p + 2]) {
			best = MIN_MATCH;
			best_pos = c;
			break;
		}
		cb3 -= (uint32_t) prev3[cb3 & (uint32_t) DEF_WMASK];
	}

	match_t r;
	r.len = 0;
	r.dist = 0;
	if (best > least) {
		r.len = best;
		r.dist = (int32_t)(p - best_pos);
	}
	return r;
}

// Find a match beginning at p, from hash table entries.
match_t find_match(finder_t *f, size_t p, uint32_t cand, uint32_t cand3, int32_t depth, int32_t best) {
	if (f->narrow) {
		return find_match16(f, p, cand, cand3, depth, best);
	}
	uint8_t *win = f->win;
	int32_t maxlen = (int32_t) min(f->win_len - p, (size_t) MAX_MATCH);
	int32_t least = best;
	size_t best_pos = 0;
	uint32_t here = (uint32_t) p + 1 + f->stamp;
	if (best >= maxlen) {
		cand = 0; // nothing longer fits
	}
	uint32_t p4 = 0;
	if (cand) {
		p4 = load32(win + p);
	}

	// Until a first match
	for (; best < MIN_MATCH && cand && depth > 0; depth--) {
		uint32_t dist = here - cand;
		if (dist > (uint32_t) DEF_WSIZE) {
			cand = 0;
			break;
		}
		size_t c = p - (size_t) dist;
		if (dist == (uint32_t) DEF_WSIZE) {
			depth = 1;
		}
		cand = f->prev[entry_slot(f, cand)];
		uint32_t x = load32(win + c) ^ p4;
		if (!(x & 0xffffff)) {
			if (x) {
				best = 3;
			} else {
				best = 4 + match_len(win + c + 4, win + p + 4, maxlen - 4);
			}
			best_pos = c;
			if (best >= f->lvl.nice || best == maxlen) {
				cand = 0;
				break;
			}
			if (best >= f->lvl.good && depth > 8) {
				depth = 8;
			}
		}
	}

	// Once best >= MIN_MATCH
	for (; cand && depth > 0; depth--) {
		uint32_t dist = here - cand;
		if (dist > (uint32_t) DEF_WSIZE) {
			break;
		}
		size_t c = p - (size_t) dist;
		if (dist == (uint32_t) DEF_WSIZE) {
			depth = 1;
		}
		cand = f->prev[entry_slot(f, cand)];
		if (load32(win + c + best - 3) == load32(win + p + best - 3) && load32(win + c) == p4) {
			int32_t len = 4 + match_len(win + c + 4, win + p + 4, maxlen - 4);
			if (len > best) {
				best = len;
				best_pos = c;
				if (best >= f->lvl.nice || best == maxlen) {
					break;
				}
				if (best >= f->lvl.good && depth > 8) {
					depth = 8;
				}
			}
		}
	}

	// Fall back to short, close 3-byte matches
	int32_t depth3 = 0;
	if (best < MIN_MATCH) {
		depth3 = f->lvl.depth3;
	}
	for (; cand3 && depth3 > 0; depth3--) {
		uint32_t dist = here - cand3;
		if (dist > (uint32_t) D3_MAX) {
			break;
		}
		size_t c = p - (size_t) dist;
		if (win[c] == win[p] && win[c + 1] == win[p + 1] && win[c + 2] == win[p + 2]) {
			best = MIN_MATCH;
			best_pos = c;
			break;
		}
		cand3 = f->prev3[entry_slot(f, cand3)];
	}

	match_t r;
	r.len = 0;
	r.dist = 0;
	if (best > least) {
		r.len = best;
		r.dist = (int32_t)(p - best_pos);
	}
	return r;
}

// Find the best match at p longer than best, inserting p, the next
// position to insert, into the hash chains.
match_t match_at(finder_t *f, size_t p, int32_t depth, int32_t best) {
	if (p < f->lim) {
		heads_t c = hash_insert(f);
		return find_match(f, p, c.c4, c.c3, depth, best);
	}
	f->ins = p + 1;
	if (f->use3 && p + (size_t) MIN_MATCH <= f->win_len) {
		uint32_t v = (uint32_t) f->win[p] | ((uint32_t) f->win[p + 1] << 8) | ((uint32_t) f->win[p + 2] << 16);
		return find_match(f, p, 0, f->head3[hash3(v)], depth, best);
	}
	match_t r;
	r.len = 0;
	r.dist = 0;
	return r;
}

// Decide whether 3-byte matching is worthwhile by sampling how often
// 4-byte hashes collide in the first part of the input.
void sample(deflator_t *d) {
	d->sampled = 1;
	size_t n = min(d->win_len, 32768);
	if (n < 64) {
		return;
	}
	uint32_t *seen = d->seen;
	uint32_t hits = 0;
	for (size_t i = 0; i + 4 <= n; i++) {
		uint32_t h = hash4(load32(d->win + i));
		uint32_t bit = (uint32_t) 1 << (h & 31);
		if (seen[h >> 5] & bit) {
			hits++;
		}
		seen[h >> 5] |= bit;
	}
	memset(seen, 0, HASH_SIZE / 8);
	if ((uint64_t) hits * 10 < (uint64_t)(n - 3) * 7) {
		d->use3 = 1;
	} else {
		d->use3 = 0;
	}
}

// Parse with use3, lazy, and narrow as arguments.
void parse_with(deflator_t *d, size_t end, int32_t use3, int32_t lazy, int32_t narrow) {
	finder_t f;
	f.win = d->win;
	f.head = d->head;
	f.prev = d->prev;
	f.head3 = d->head3;
	f.prev3 = d->prev3;
	f.win_len = d->win_len;
	f.lim = 0;
	if (d->win_len >= (size_t) HASH_LEN) {
		f.lim = d->win_len - (size_t)(HASH_LEN - 1);
	}
	f.ins = d->ins;
	f.base = (uint32_t) d->base;
	f.stamp = d->stamp;
	f.use3 = use3;
	f.lvl = d->lvl;
	f.narrow = narrow;
	if (!lazy) {
		f.lvl.lazy = 0;
	}
	hash_ahead(&f, f.ins);

	size_t p = d->pos;
	while (p < end && !d->blkready) {
		match_t m = match_at(&f, p, f.lvl.depth, MIN_MATCH - 1);

		// Lazy matching: prefer a longer match starting one byte later
		for (int32_t step = 0; step < f.lvl.lazy; step++) {
			if (m.len < MIN_MATCH || m.len >= f.lvl.good || p + 1 >= f.win_len) {
				break;
			}
			match_t m2 = match_at(&f, p + 1, f.lvl.depth >> 1, m.len);
			if (m2.len <= m.len) {
				break;
			}
			tok_lit(d, p);
			p++;
			m = m2;
		}

		if (m.len >= MIN_MATCH) {
			tok_match(d, p, m.len, m.dist);
			if (m.len <= f.lvl.insert) {
				ensure_insert(&f, p + (size_t) m.len - 1);
			} else {
				f.ins = max(f.ins, p + (size_t) m.len);
				hash_ahead(&f, f.ins);
			}
			p += (size_t) m.len;
		} else {
			tok_lit(d, p);
			p++;
		}
	}
	d->pos = p;
	d->ins = f.ins;
}

// The bucket finder's state, in a local as the chains' finder is.
typedef {
	uint8_t *win;
	uint32_t *t;
	token_t *tok; // next token
	uint32_t *lit_freq;
	uint32_t *dist_freq;
	deflator_t *d;
	size_t lim; // positions below have HASH_LEN bytes to hash
	uint32_t stamp1; // stamp plus one
	uint32_t h; // hash of the position in hand, when below lim
	int32_t nice;
	int32_t ins;
} bfinder_t;

uint32_t *bkt(bfinder_t *f, uint32_t h, int32_t nb) {
	return f->t + (size_t) h * (size_t) nb;
}

// Search and insert position p, below lim, and emit its token.
size_t bkt_step(bfinder_t *f, size_t p, int32_t nb, int32_t last) {
	uint8_t *win = f->win;
	uint32_t *b = bkt(f, f->h, nb);
	uint32_t c0 = b[0];
	uint32_t c1 = 0;
	if (nb == 2) {
		c1 = b[1];
	}
	uint32_t here = (uint32_t) p + f->stamp1;
	if (nb == 2) {
		b[1] = c0;
	}
	b[0] = here;

	uint32_t hn = 0;
	if (!last || p + 1 < f->lim) {
		hn = hash4(load32(win + p + 1));
	}

	uint32_t maxd = (uint32_t) min(p, (size_t) DEF_WSIZE);
	uint32_t p4 = load32(win + p);
	int32_t best = 0;
	uint32_t bestd = 0;
	uint32_t dist = here - c0;
	int32_t maxlen = 0;
	if (dist - 1 < maxd && load32(win + p - (size_t) dist) == p4) {
		maxlen = (int32_t) min(f->d->win_len - p, (size_t) MAX_MATCH);
		best = 4 + match_len(win + p - (size_t) dist + 4, win + p + 4, maxlen - 4);
		bestd = dist;
	}
	if (nb == 2 && best < f->nice) {
		dist = here - c1;
		if (dist - 1 < maxd) {
			const uint8_t *c = win + p - (size_t) dist;
			if (!best) {
				if (load32(c) == p4) {
					maxlen = (int32_t) min(f->d->win_len - p, (size_t) MAX_MATCH);
					best = 4 + match_len(c + 4, win + p + 4, maxlen - 4);
					bestd = dist;
				}
			} else if (best < maxlen && load32(c + best - 3) == load32(win + p + best - 3) && load32(c) == p4) {
				int32_t len = 4 + match_len(c + 4, win + p + 4, maxlen - 4);
				if (len > best) {
					best = len;
					bestd = dist;
				}
			}
		}
	}

	deflator_t *d = f->d;
	if (!best) {
		token_t t;
		t.litlen = (uint8_t) p4;
		t.dist = 0;
		*f->tok++ = t;
		f->lit_freq[(uint8_t) p4]++;
		f->h = hn;
		return p + 1;
	}

	token_t t;
	t.litlen = (uint16_t) best;
	t.dist = (uint16_t) bestd;
	*f->tok++ = t;
	f->lit_freq[257 + lcode_of(d, best)]++;
	f->dist_freq[dcode_of(d, (int32_t) bestd)]++;
	size_t q = p + (size_t) best;
	uint32_t hq = 0;
	if (q < f->lim) {
		hq = hash4(load32(win + q));
	}
	if (best <= f->ins) {
		// Insert the match's later positions, hashing ahead
		size_t xe = min(q, f->lim);
		uint32_t hx = hn;
		for (size_t x = p + 1; x < xe; x++) {
			uint32_t hy = hq;
			if (x + 1 < xe) {
				hy = hash4(load32(win + x + 1));
			}
			uint32_t *bx = bkt(f, hx, nb);
			if (nb == 2) {
				bx[1] = bx[0];
			}
			bx[0] = (uint32_t) x + f->stamp1;
			hx = hy;
		}
	}
	f->h = hq;
	return q;
}

void parse_bucket_with(deflator_t *d, size_t end, int32_t nb) {
	size_t p = d->pos;
	bfinder_t f;
	f.win = d->win;
	f.t = d->head;
	f.tok = d->toks + d->ntok;
	f.lit_freq = d->lit_freq;
	f.dist_freq = d->dist_freq;
	f.d = d;
	f.lim = 0;
	if (d->win_len >= (size_t) HASH_LEN) {
		f.lim = d->win_len - (size_t)(HASH_LEN - 1);
	}
	f.stamp1 = d->stamp + 1;
	f.nice = d->lvl.nice;
	f.ins = d->lvl.insert;
	f.h = 0;
	if (!d->ntok) {
		d->blk_start = d->base + (uint64_t) p;
	}
	// Blocks grow by the input parsed, so the cap is a position.
	size_t p0 = p;
	size_t cap = p + (size_t)((uint64_t) BLOCK_CAP - d->blk_len);
	end = min(end, cap);
	size_t stop = 0;
	if (f.lim) {
		stop = min(end, f.lim - 1);
	}
	if (p < f.lim) {
		f.h = hash4(load32(f.win + p));
	}
	while (p < stop) {
		p = bkt_step(&f, p, nb, 0);
	}
	while (p < end) {
		if (p < f.lim) {
			p = bkt_step(&f, p, nb, 1);
		} else {
			token_t t;
			t.litlen = f.win[p];
			t.dist = 0;
			*f.tok++ = t;
			f.lit_freq[f.win[p]]++;
			p++;
		}
	}
	d->ntok = f.tok - d->toks;
	d->blk_len += (uint64_t)(p - p0);
	d->blkready = d->blk_len >= (uint64_t) BLOCK_CAP;
	d->pos = p;
	d->ins = p;
}

void parse(deflator_t *d, size_t end) {
	if (d->lvl.bucket) {
		d->sampled = 1;
		if (d->lvl.bucket == 2) {
			parse_bucket_with(d, end, 2);
		} else {
			parse_bucket_with(d, end, 1);
		}
		return;
	}
	if (!d->sampled) {
		sample(d);
	}
	if (d->narrow) {
		parse_with(d, end, d->use3, 1, 1);
	} else if (d->lvl.lazy) {
		parse_with(d, end, d->use3, 1, 0);
	} else {
		parse_with(d, end, d->use3, 0, 0);
	}
}

// Rebase a table's entries, emptying those whose positions fall out of
// the window or were forgotten, either way at or below lim.
void slide_table(uint32_t *t, int32_t n, uint32_t lim, uint32_t shift) {
	for (int32_t i = 0; i < n; i++) {
		uint32_t v = t[i];
		if (v > lim) {
			t[i] = v - shift;
		} else {
			t[i] = 0;
		}
	}
}

// The same for a link table. (Distance links need nothing.)
void slide_links(uint32_t *t, uint32_t lim, uint32_t shift) {
	for (size_t i = 0; i < (size_t) DEF_WSIZE; i++) {
		uint32_t v = t[i];
		if (v > lim) {
			t[i] = v - shift;
		} else {
			t[i] = 0;
		}
	}
}

// Whether the next slide would discard data of the block in progress.
int32_t slide_ends_block(deflator_t *d) {
	if (!d->ntok || d->pos <= (size_t) DEF_WSIZE) {
		return 0;
	}
	size_t shift = d->pos - (size_t) DEF_WSIZE;
	return d->blk_start < d->base + (uint64_t) shift;
}

// Discard window contents more than WSIZE behind the parse position.
void slide(deflator_t *d) {
	if (d->pos <= (size_t) DEF_WSIZE) {
		return;
	}
	size_t shift = d->pos - (size_t) DEF_WSIZE;
	if (d->pend_len && d->pend_start < d->base + (uint64_t) shift) {
		flush_pending(d, 0);
	}
	memmove(d->win, d->win + shift, d->win_len - shift);
	d->base += (uint64_t) shift;
	d->win_len -= shift;
	d->pos -= shift;
	d->ins -= shift;

	uint32_t s = (uint32_t) shift;
	uint32_t lim = d->stamp + s;
	if (d->lvl.bucket) {
		slide_table(d->head, HASH_SIZE * d->lvl.bucket, lim, s);
		return;
	}
	slide_table(d->head, HASH_SIZE, lim, s);
	if (!d->narrow) {
		slide_links(d->prev, lim, s);
	}
	if (d->use3) {
		slide_table(d->head3, HASH_SIZE, lim, s);
		if (!d->narrow) {
			slide_links(d->prev3, lim, s);
		}
	}
}

// Forget all history by emptying the hash chains.
void forget(deflator_t *d) {
	size_t lim = 0;
	if (d->win_len >= (size_t) HASH_LEN) {
		lim = d->win_len - (size_t)(HASH_LEN - 1);
	}
	size_t n = min(d->ins, lim);
	if (n <= (size_t) FORGET_REHASH) {
		for (size_t p = 0; p < n; p++) {
			uint32_t v = load32(d->win + p);
			uint32_t h = hash4(v);
			d->head[h] = 0;
			d->head3[hash3(v)] = 0;
			d->head[2 * h] = 0; // two-entry buckets
			d->head[2 * h + 1] = 0;
		}
	} else if (d->stamp < STAMP_LAST) {
		d->stamp += STAMP_STEP;
	} else {
		memset(d->head, 0, 2 * HASH_SIZE * sizeof(uint32_t));
		d->stamp = 0;
	}
}

// Prepare to compress a new stream at the same level, unless changed
// with setlevel. This costs time in proportion to the previous stream.
void reset(deflator_t *d) {
	forget(d);
	d->olen = 0;
	d->ooff = 0;
	d->blkready = 0;
	d->flushed = 0;
	d->flushing = NONE;
	d->finq = 0;
	d->nqueued = 0;
	d->win_len = 0;
	d->base = 0;
	d->pos = 0;
	d->ins = 0;
	d->sampled = 0;
	d->use3 = 0;
	d->blk_start = 0;
	d->pend_start = 0;
	d->pend_len = 0;
	d->bitbuf = 0;
	d->bitcnt = 0;
	clear_block(d);
}

// Levels 1 through 9; others are clamped. Only at the start of a stream.
void setlevel(deflator_t *d, int32_t level) {
	level = max(1, min(level, 9));
	d->lvl = deflate_levels[level];
	if (level >= NARROW_LEVEL) {
		d->narrow = 1;
	} else {
		d->narrow = 0;
	}
}

// Tokens are written before they are read, and chains lead only to links
// that insertions wrote.
pub deflator_t *deflate_create(int32_t level) {
	if (level < 1 || level > 9) {
		return 0;
	}
	deflator_t *d = calloc!(1, (sizeof(deflator_t)));
	setlevel(d, level);
	d->obuf = calloc!((size_t) DEF_STAGE, 1);
	d->win = calloc!((size_t) WIN_CAP, 1);
	d->head = calloc!(2 * (size_t) HASH_SIZE, (sizeof(uint32_t)));
	d->head3 = d->head + HASH_SIZE;
	d->prev = calloc!((size_t) DEF_WSIZE, (sizeof(uint32_t)));
	d->seen = calloc!((size_t) HASH_SIZE / 32, (sizeof(uint32_t)));
	d->prev3 = calloc!((size_t) DEF_WSIZE, (sizeof(uint32_t)));
	d->toks = calloc!((size_t) TOK_CAP, (sizeof(token_t)));

	for (int32_t i = 0; i < 288; i++) {
		if (i < 144) {
			d->fixlit.len[i] = 8;
		} else if (i < 256) {
			d->fixlit.len[i] = 9;
		} else if (i < 280) {
			d->fixlit.len[i] = 7;
		} else {
			d->fixlit.len[i] = 8;
		}
	}
	huff_codes(&d->fixlit, 288);
	for (int32_t i = 0; i < NDIST; i++) {
		d->fixdist.len[i] = 5;
	}
	huff_codes(&d->fixdist, NDIST);

	for (int32_t len = MIN_MATCH; len <= MAX_MATCH; len++) {
		d->lcode[len] = (uint8_t) length_code(len);
	}
	for (int32_t dist = 1; dist <= 256; dist++) {
		d->dcode[dist - 1] = (uint8_t) dist_code(dist);
	}
	for (int32_t i = 2; i < 256; i++) {
		d->dcode[256 + i] = (uint8_t) dist_code((i << 7) + 1);
	}
	reset(d);
	return d;
}

// Free an encoder state.
pub void deflate_free(deflator_t *d) {
	if (d) {
		free(d->toks);
		free(d->prev3);
		free(d->seen);
		free(d->prev);
		free(d->head);
		free(d->win);
		free(d->obuf);
		free(d);
	}
}

// Ensure staging room for one emission step, compacting only if that
// makes the room.
int32_t def_room(deflator_t *d) {
	size_t room = (size_t) DEF_STAGE - d->olen;
	if (room < (size_t) DEF_STAGE_NEED && room + d->ooff >= (size_t) DEF_STAGE_NEED) {
		memmove(d->obuf, d->obuf + d->ooff, d->olen - d->ooff);
		d->olen -= d->ooff;
		d->ooff = 0;
	}
	return (size_t) DEF_STAGE - d->olen >= (size_t) DEF_STAGE_NEED;
}

// Staged output not yet handed out.
void pending(deflator_t *d, uint8_t **buf, size_t *len) {
	*buf = d->obuf + d->ooff;
	*len = d->olen - d->ooff;
}

// Mark the first n bytes of pending output as handed out.
void consume(deflator_t *d, size_t n) {
	d->ooff += n;
	if (d->ooff == d->olen) {
		d->ooff = 0;
		d->olen = 0;
	}
}

void def_drain(deflator_t *d, zbuf *b) {
	uint8_t *p;
	size_t plen;
	pending(d, &p, &plen);
	size_t n = min(plen, b->outlen);
	if (n) {
		memcpy(b->out, p, n);
		b->out += n;
		b->outlen -= n;
		consume(d, n);
	}
}

// Emit everything parsed so far for the given flush.
void def_flush(deflator_t *d, int32_t flush) {
	switch (flush) {
		case FINISH: {
			flush_block(d, 1);
			bw_align(d);
		}
		case SYNC: {
			flush_block(d, 0);
			flush_pending(d, 0);
			emit_stored_chunk(d, 0, 0, 0);
		}
		case FULL: {
			flush_block(d, 0);
			flush_pending(d, 0);
			emit_stored_chunk(d, 0, 0, 0);
			forget(d);
			d->ins = d->pos;
		}
	}
}

// The mode of the latest flush due, or NONE if none is.
int32_t def_lastdue(deflator_t *d) {
	if (d->finq) {
		return FINISH;
	}
	if (d->nqueued & 1) {
		return SYNC + FULL - d->flushing;
	}
	return d->flushing;
}

// Compress from b->in into b->out, advancing both.
int32_t stream(deflator_t *d, zbuf *b, int32_t flush) {
	if ((d->flushing == FINISH || d->finq) && (b->inlen || flush != FINISH)) {
		return EUSAGE;
	}

	if (d->flushing && flush && !b->inlen && flush != def_lastdue(d)) {
		if (flush == FINISH) {
			d->finq = 1;
		} else {
			d->nqueued++;
		}
	}

	while (true) {
		def_drain(d, b);
		if (d->flushed) {
			if (d->ooff < d->olen) {
				return NEED_OUTPUT;
			}
			if (d->flushing == FINISH) {
				return DONE;
			}
			d->flushed = 0;
			if (d->nqueued) {
				d->nqueued--;
				d->flushing = SYNC + FULL - d->flushing;
				continue;
			}
			if (d->finq) {
				d->finq = 0;
				d->flushing = FINISH;
				continue;
			}
			int32_t repeat = flush == d->flushing && !b->inlen;
			d->flushing = NONE;
			if (repeat) {
				return DONE;
			}
		}
		if (!d->flushing && !b->inlen) {
			d->flushing = flush;
		}

		if (d->blkready) {
			if (!def_room(d)) {
				return NEED_OUTPUT;
			}
			flush_block(d, 0);
			d->blkready = 0;
		} else if (d->win_len == (size_t) WIN_CAP) {
			parse(d, WIN_CAP - LOOKAHEAD);
			if (!d->blkready && slide_ends_block(d)) {
				d->blkready = 1;
			} else if (!d->blkready) {
				if (!def_room(d)) {
					return NEED_OUTPUT;
				}
				slide(d);
			}
		} else if (d->flushing) {
			parse(d, d->win_len);
			if (!d->blkready) {
				if (!def_room(d)) {
					return NEED_OUTPUT;
				}
				def_flush(d, d->flushing);
				d->flushed = 1;
			}
		} else if (b->inlen) {
			size_t n = min(b->inlen, (size_t) WIN_CAP - d->win_len);
			memcpy(d->win + d->win_len, b->in, n);
			d->win_len += n;
			b->in += n;
			b->inlen -= n;
		} else {
			return NEED_INPUT;
		}
	}
}

// Compress from b->in into b->out with a flush mode, advancing both.
pub int32_t deflate(deflator_t *d, zbuf *b, int32_t flush) {
	if (!d || !b || (!b->in && b->inlen) || (!b->out && b->outlen) || flush < NONE || flush > FINISH) {
		return EUSAGE;
	}
	return stream(d, b, flush);
}

// Starts a new stream.
pub int32_t deflate_reset(deflator_t *d, int32_t level) {
	if (!d || level < 1 || level > 9) {
		return EUSAGE;
	}
	reset(d);
	setlevel(d, level);
	return DONE;
}

// Appends bytes at a byte boundary to the pending compressed output. A
// container uses this for its header and trailer: call it after a reset,
// or when a FINISH call has returned DONE, then call deflate
// again with FINISH to hand the bytes to the caller.
pub void deflate_append(deflator_t *d, const uint8_t *p, size_t len) {
	if (d) {
		bytes(d, p, len);
	}
}
