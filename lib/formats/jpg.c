#import bits
#import compress/huffman
#import enc/endian
#import image
#import reader
// #import dbg
#import formats/tiff

pub typedef {
	bool set;
	char msg[100];
} err_t;

void seterr(err_t *err, const char *fmt, ...) {
	va_list args = {};
	va_start(args, fmt);
	vsnprintf(err->msg, sizeof(err->msg), fmt, args);
	va_end(args);
	err->set = true;
}

const double PI = 3.141592653589793238462643383279502884197169399375105820974944;

const char *ids[] = { "invalid (0)", "Y", "Cb", "Cr", "I", "Q" };

pub typedef {
	uint8_t id; // 1 = Y, 2 = Cb, 3 = Cr, 4 = I, 5 = Q
	uint8_t hsize; // how many 8x8 blocks per unit horizontally
	uint8_t vsize; // how many 8x8 blocks per unit vertically
	uint8_t qtable_index; // which quantization table to use
} component_t;

pub typedef {
	image.image_t *img;
	huffman.tree_t *htables[100];
	uint8_t *quant[100];
	uint16_t restart_interval; // how often to reset the decoder, in MCUs.
	uint8_t ncomponents;
	component_t components[3];
} jpeg_t;

pub jpeg_t *read(const char *path, err_t *err) {
	FILE *f = fopen(path, "rb");
	reader.t *R = reader.file(f);
	jpeg_t *self = calloc!(1, sizeof(jpeg_t));
	while (true) {
		uint16_t hdr = 0;
		endian.read2be(R, &hdr);
		if (hdr == 0) break;
		if (hdr == 0xffd9) {
			break;
		}
		switch (hdr) {
			case 0xffe1: { read_app1(self, R); }
			case 0xffe2: { read_app2(self, R); }
			case 0xffe4: { read_app4(self, R); }
			case 0xffe0: { read_appdef(self, R); }
			case 0xffdb: { read_quant_table(self, R); }
			case 0xffdd: { read_restart_interval(self, R, err); }
			case 0xffc0: { read_baseline_dct(self, R, err); }
			case 0xffc4: { read_huffman_table(self, R); }
			case 0xffda: { read_scan(self, R); }
			case 0xffd8: {}
			default: { panic("unknown header %x", hdr); }
		}
		if (err->set) {
			OS.free(self);
			reader.free(R);
			fclose(f);
			return NULL;
		}
	}
	reader.free(R);
	fclose(f);
	return self;
}

pub void free(jpeg_t *j) {
	OS.free(j);
}

void read_app1(jpeg_t *self, reader.t *r) {
	(void) self;
	uint16_t len;
	endian.read2be(r, &len);
	printf("App1 (len=%u)\n", len);

	uint8_t *buf = calloc!(len-2, 1);
	reader.read(r, buf, len-2);

	if (strcmp((char *) buf, "Exif") == 0 && buf[4] == 0 && buf[5] == 0) {
		tiff.file_t *tf = tiff.parse(buf + 6, len-2-6);
		uint32_t gpspos = 0;
		for (size_t i = 0; i < tf->ndirs; i++) {
			tiff.dir_t *d = tf->dirs[i];
			for (size_t j = 0; j < d->nentries; j++) {
				if (d->entries[j]->tag == 34853) {
					gpspos = d->entries[j]->value;
					break;
				}
			}
		}
		if (gpspos != 0) {
			tiff.setpos(tf, gpspos);
			tiff.read_dir(tf);
		}
		for (size_t i = 0; i < tf->ndirs; i++) {
			tiff.dir_t *d = tf->dirs[i];
			dumpdir(d);
		}
	}
	// dbg.print_bytes(buf, len-2);
	OS.free(buf);

	// E  x  i  f \0 \0
	// ...
	// reader.skip(r, len-2);
}

void dumpdir(tiff.dir_t *d) {
	printf("%zu entries\n", d->nentries);
	for (size_t ei = 0; ei < d->nentries; ei++) {
		tiff.entry_t *e = d->entries[ei];
		const char *k = tiff.tagname(e->tag);
		if (!k) {
			printf("unknown tag %d\n", e->tag);
			continue;
		}
		printf("\t%s\t", k);
		switch (e->type) {
			case tiff.ASCII: {
				char *s = tiff.getstring(e);
				printf("\"%s\"", s);
				OS.free(s);
			}
			case tiff.RATIONAL: {
				tiff.setpos(e->file, e->value);
				for (size_t i = 0; i < e->count; i++) {
					double r = tiff.read_rational(e->file);
					printf("%g ", r);
				}
			}
			default: {
				printf("%u (%zu)", e->value, e->count);
			}
		}
		printf("\n");
	}
}

void read_app2(jpeg_t *self, reader.t *r) {
	(void) self;
	uint16_t len;
	endian.read2be(r, &len);
	printf("App2 (len=%u)\n", len);

	// ICC profiles
	// ...
	reader.skip(r, len-2);
}

void read_app4(jpeg_t *self, reader.t *r) {
	(void) self;
	uint16_t len;
	endian.read2be(r, &len);
	printf("App4 (len=%u)\n", len);
	// ?
	reader.skip(r, len-2);
}

void read_appdef(jpeg_t *self, reader.t *r) {
	(void) self;
	uint16_t len;
	endian.read2be(r, &len);
	printf("Application Default Header (len=%u)\n", len);
	reader.skip(r, len-2);
}

void read_restart_interval(jpeg_t *self, reader.t *r, err_t *err) {
	uint16_t len;
	if (!endian.read2be(r, &len)) {
		seterr(err, "failed to read section length");
		return;
	}
	if (len != 4) {
		seterr(err, "restart interval: expected len=4, got %u", len);
		return;
	}
	if (!endian.read2be(r, &self->restart_interval)) {
		seterr(err, "failed to read restart interval");
		return;
	}
	printf("restart interval len=%u, val=%u\n", len, self->restart_interval);
}

void read_quant_table(jpeg_t *self, reader.t *r) {
	uint16_t len;
	uint8_t params;
	endian.read2be(r, &len);
	reader.read(r, &params, 1);
	int precision = (params >> 4) & 0xf;
    int num = params & 0xf; // 0 = luma, 1 = chroma

    printf("Quantization Table (num=%d, prec=%d)\n", num, precision);

	// QT values, n = 64*(precision+1)
	if (precision != 0) {
        // the precision of QT, 0 = 8 bit, otherwise 16 bit
        panic("Quantization table presision %d not implemented", precision);
    }
	uint8_t *qt = calloc!(64, 1);
	reader.read(r, qt, 64);
	self->quant[num] = qt;
}



void read_baseline_dct(jpeg_t *self, reader.t *r, err_t *err) {
	uint16_t len = 0;
	endian.read2be(r, &len);
	printf("dct len = %d\n", len);

	uint8_t precision;
	uint16_t h;
	uint16_t w;
	uint8_t components;
	reader.read(r, &precision, 1);
	endian.read2be(r, &h);
	endian.read2be(r, &w);
	reader.read(r, &components, 1);

	printf("\tprecision = %u bits\n", precision);
	printf("\tsize = %u x %u\n", w,  h);
	if (components != 3) {
		panic("ncomponents = %u not implemented", components);
	}
	self->ncomponents = components;
	for (uint8_t i = 0; i < components; i++) {
		component_t *c = &self->components[i];
		reader.read(r, &c->id, 1);

		uint8_t sampling_factors; // horizontal | vertical: (h << 4) | v
		reader.read(r, &sampling_factors, 1);
		c->hsize = sampling_factors >> 4;
		c->vsize = sampling_factors & 0xf;

		reader.read(r, &c->qtable_index, 1);
	}

	for (uint8_t i = 0; i < self->ncomponents; i++) {
		component_t *c = &self->components[i];
		uint8_t id = c->id;
		printf("\tcomponent %u: id=%u (%s)", i, id, ids[id]);
		printf(" sampling_factors=%d,%d qtable_num=%u\n", c->hsize, c->vsize, c->qtable_index);
		if (c->vsize != 1 || c->hsize != 1) {
			seterr(err, "sampling factors != 1 not implemented");
			// return;
		}
		if (i == 0 && id != 1) {
			panic("expected component %d, got %u", 1, id);
		}
		if (i == 1 && id != 2) {
			panic("expected component %d, got %u", 2, id);
		}
		if (i == 2 && id != 3) {
			panic("expected component %d, got %u", 3, id);
		}
	}
	if (err->set) {
		return;
	}
	self->img = image.new(w, h);
}

void read_huffman_table(jpeg_t *self, reader.t *r) {
	uint16_t len = 0;
	uint8_t id;
	uint8_t lengths[16] = {};

	endian.read2be(r, &len);
	reader.read(r, &id, 1);
	reader.read(r, lengths, 16);

	size_t sum = 0;
	for (int i = 0; i < 16; i++) {
		sum += lengths[i];
	}

	uint8_t *elements = calloc!(sum, 1);
	int epos = 0;
	for (int a = 0; a < 16; a++) {
		uint8_t len = lengths[a];
		for (uint8_t i = 0; i < len; i++) {
			uint8_t x;
			reader.read(r, &x, 1);
			elements[epos++] = x;
		}
	}
	self->htables[id] = huffman.treefrom(lengths, 16, elements);

	int tid = id & 0xf; // th, id
    int tclass = id >> 4; // class (0=dc, 1=ac)
    printf("Huffman Table id=%d class=%d\n", tid, tclass);
	// switch (tclass) {
    //     case 0: { info->dc_huff_trees[tid] = t; }
    //     case 1: { info->ac_huff_trees[tid] = t; }
    // }
	OS.free(elements);
}

void read_scan(jpeg_t *self, reader.t *r) {
	uint16_t len = 0;
	endian.read2be(r, &len);
	if (len != 12) {
		panic("expected scan header length 12, got %u", len);
	}

	//
	// read scan header
	//
	printf("scan header\n");
	uint8_t ncomp;
	reader.read(r, &ncomp, 1);
	for (uint8_t i = 0; i < ncomp; i++) {
        uint8_t id;
		uint8_t wtf;
        reader.read(r, &id, 1);
        reader.read(r, &wtf, 1);
        int dc_table_id = wtf >> 4;
        int ac_table_id = wtf & 0xf;
		printf("\tcomponent %u: dctable=%d actable=%d\n", i, dc_table_id, ac_table_id);
	}

	uint8_t ss;
	uint8_t se;
	uint8_t ahal;
    reader.read(r, &ss, 1);
	reader.read(r, &se, 1);
	reader.read(r, &ahal, 1);
    printf("ss=%u se=%u ah/al=%u\n", ss, se, ahal);

	reader.t *e = escreader(r);
	read_scan_data(self, e);
	reader.free(e);
}

void read_scan_data(jpeg_t *self, reader.t *r) {
	bits.reader_t *br = bits.newreader(r);

	// First values ("DC") are diff-encoded across all blocks.
	// These will contain the current values.
	int dc[3] = {0,0,0};

	// Read n blocks and tile them into the main image left to right.
	int w = self->img->width;
	int h = self->img->height;
	int ri = self->restart_interval;
	image.image_t *block = image.new(8, 8);
	int x = 0;
	int y = 0;
	int n = (h/8) * (w/8);
	for (int i = 0; i < n; i++) {
		ri--;
		// printf("ri=%d\n", ri);
		readunit(self, dc, br, block);
		image.paste(self->img, block, x, y);
		x += 8;
		if (x >= w) {
			x = 0;
			y += 8;
		}
	}
	image.free(block);
}

void readunit(jpeg_t *self, int *dc, bits.reader_t *br, image.image_t *block) {
	// Read 3 component blocks
	int vals1[64] = {};
	int vals2[64] = {};
	int vals3[64] = {};

	// Y
	huffman.reader_t *hrdc = huffman.newreader(self->htables[0], br);
	huffman.reader_t *hrac = huffman.newreader(self->htables[16], br);
	readblock(br, hrdc, hrac, dc[0], vals1);
	huffman.closereader(hrdc);
	huffman.closereader(hrac);
	dc[0] = vals1[0];

	// B
	hrdc = huffman.newreader(self->htables[1], br);
	hrac = huffman.newreader(self->htables[17], br);
	readblock(br, hrdc, hrac, dc[1], vals2);
	huffman.closereader(hrdc);
	huffman.closereader(hrac);
	dc[1] = vals2[0];

	// R
	hrdc = huffman.newreader(self->htables[1], br);
	hrac = huffman.newreader(self->htables[17], br);
	readblock(br, hrdc, hrac, dc[2], vals3);
	huffman.closereader(hrdc);
	huffman.closereader(hrac);
	dc[2] = vals3[0];

	// Undo quantization
	uint8_t *quant1 = self->quant[self->components[0].qtable_index];
	uint8_t *quant2 = self->quant[self->components[1].qtable_index];
	uint8_t *quant3 = self->quant[self->components[2].qtable_index];
	for (int i = 0; i < 64; i++) {
		vals1[i] *= quant1[i];
		vals2[i] *= quant2[i];
		vals3[i] *= quant3[i];
	}

	// Undo zigzag
	undozz(vals1);
	undozz(vals2);
	undozz(vals3);

	// Rebuild the components
	double Y[64] = {};
	double Cb[64] = {};
	double Cr[64] = {};
	rebuild(vals1, Y);
	rebuild(vals2, Cb);
	rebuild(vals3, Cr);

	// Compose RGB
	image.rgba_t vv[64];
	for (int i = 0; i < 64; i++) {
		vv[i] = toRGB(Y[i], Cr[i], Cb[i]);
	}

	// Draw the block
	int pos = 0;
	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 8; x++) {
			image.rgba_t val = vv[pos++];
			image.set(block, x, y, val);
		}
	}
}

void rebuild(int *weights, double *out) {
	double shape[64];
	for (int i = 0; i < 64; i++) {
		int w = weights[i];
		if (w == 0) continue;
		int n = i & 0x7;
		int m = i >> 3;
		getshape(shape, n, m);
		mmulk(shape, (double) w);
		madd(out, shape);
	}
}

const uint8_t zigzag[] = {
	0,   1,  8, 16,  9,  2,  3, 10,
	17, 24, 32, 25, 18, 11,  4,  5,
	12, 19,	26, 33, 40, 48, 41, 34,
	27, 20, 13,  6,  7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36,
	29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46,
	53, 60, 61, 54, 47, 55, 62, 63};

void undozz(int *vals) {
	int tmp[64] = {};
	for (int i = 0; i < 64; i++) {
		tmp[zigzag[i]] = vals[i];
	}
	for (int i = 0; i < 64; i++) {
		vals[i] = tmp[i];
	}
}

void mmulk(double *m, double k) {
	for (int i = 0; i < 64; i++) {
		m[i] *= k;
	}
}

void madd(double *s, double *m) {
	for (int i = 0; i < 64; i++) {
		s[i] += m[i];
	}
}

// Puts the standard basis shape (u, v) into res.
// u and v are indexes [0..63].
// res is a 8x8 array.
pub void getshape(double *shape, int n, m) {
	double a = 1;
	double b = 1;
	if (n == 0) a = sqrt(0.5);
	if (m == 0) b = sqrt(0.5);
	double ka = n * PI / 16.0;
	double kb = m * PI / 16.0;
	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 8; x++) {
			double nn = a * cos(ka * (2*x + 1.0));
			double mm = b * cos(kb * (2*y + 1.0));
			shape[x + 8*y] = 0.25 * nn*mm;
		}
	}
}

image.rgba_t toRGB(double Y, Cr, Cb) {
	image.rgba_t c = {};
	double R = Cr * (2 - 2 * 0.299) + Y;
	double B = Cb * (2 -2 * 0.114) + Y;
	double G = (Y - 0.114 * B - 0.299*R) / 0.587;
	c.red = clamp(R+128);
	c.green = clamp(G+128);
	c.blue = clamp(B+128);
	return c;
}

uint8_t clamp(double x) {
	if (x > 255) return 255;
	if (x < 0) return 0;
	return (uint8_t) x;
}

void readblock(bits.reader_t *br, huffman.reader_t *hrdc, *hrac, int prevdc, int *vals) {
	// DC: huff(valsize),val from raw bits
	int code = huffman.read(hrdc);
	vals[0] = prevdc + weirdonum(br, code);

	// 63 ACs: val from rle+huff+bits spaghetti
	int l = 1;
	while (l<64) {
		// huff(run,size)
		code = huffman.read(hrac);
		if (code == EOF) panic("eof");
		int run = code / 16;
        int size = code & 0xf;

		// 0,0 means end of data.
		if (run == 0 && size == 0) {
			break;
		}
		// 15,0 means 15 zeros.
		if (run == 15 && size == 0) {
			l += 15;
			continue;
		}
		l += run;
		code = size;
		if (l<64) {
			int coeff = weirdonum(br, code);
			vals[l] = coeff;
			l+=1;
		}
	}
}

int weirdonum(bits.reader_t *br, int code) {
	int bval = bits.readn(br, code);
	int l = 1 << (code-1);
	if (bval >= l) {
		return bval;
	}
	int z = 2*l-1;
	return bval - z;
}

typedef {
    reader.t *in;
    bool ended;
} escaper_t;

reader.t *escreader(reader.t *in) {
	escaper_t *e = calloc!(1, sizeof(escaper_t));
	e->in = in;
	return reader.new(e, escreadn, OS.free);
}

int escreadn(void *ctx, uint8_t *buf, size_t n) {
    escaper_t *r = ctx;
    for (size_t i = 0; i < n; i++) {
        int c = escread1(r);
        if (c == -1) return -1;
        buf[i] = (uint8_t) c;
    }
    return (int) n;
}

int escread1(escaper_t *r) {
    if (r->ended) {
		panic("reading from closed escaper");
	}
    uint8_t x;
    if (reader.read(r->in, &x, 1) != 1) {
		panic("read failed");
	}
    if (x == 0xff) {
        if (reader.read(r->in, &x, 1) != 1) {
			panic("read failed");
		}
        // 0xff 0x00 means just 0xff as data.
        if (x == 0) {
            return 0xff;
        }
        // 0xff 0xd9 means end of data.
        if (x == 0xd9) {
            r->ended = true;
            return EOF;
        }
        panic("unexpected 0xff 0x%x", x);
    }
    return x;
}
