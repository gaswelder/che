#import bits
#import compress/huffman
#import enc/endian
#import image
#import reader

const double PI = 3.141592653589793238462643383279502884197169399375105820974944;

const uint8_t zigzag[] = {
	0,   1,  8, 16,  9,  2,  3, 10,
	17, 24, 32, 25, 18, 11,  4,  5,
	12, 19,	26, 33, 40, 48, 41, 34,
	27, 20, 13,  6,  7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36,
	29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46,
	53, 60, 61, 54, 47, 55, 62, 63};

pub typedef {
	image.image_t *img;
	huffman.tree_t *htables[100];
	uint8_t *quant[100];
	int quantMapping[100];
} jpeg_t;

pub jpeg_t *read(const char *path) {
	FILE *f = fopen(path, "rb");
	reader.t *R = reader.file(f);
	jpeg_t *self = calloc!(1, sizeof(jpeg_t));
	while (true) {
		uint16_t hdr = 0;
		endian.read2be(R, &hdr);
		if (hdr == 0) break;
		printf("hdr = %x\n", hdr);
		if (hdr == 0xffd9) {
			break;
		}
		switch (hdr) {
			case 0xffe0: { read_appdef(self, R); }
			case 0xffdb: { read_quant_table(self, R); }
			case 0xffc0: { read_baseline_dct(self, R); }
			case 0xffc4: { read_huffman_table(self, R); }
			case 0xffda: { read_scan(self, R); }
			case 0xffd8: {}
			default: { panic("unknown header %x", hdr); }
		}
	}
	reader.free(R);
	fclose(f);
	return self;
}

void read_appdef(jpeg_t *self, reader.t *r) {
	(void) self;
	uint16_t len;
	endian.read2be(r, &len);
	printf("Application Default Header (len=%u)\n", len);
	reader.skip(r, len-2);
}

void read_quant_table(jpeg_t *self, reader.t *r) {
	uint16_t len;
	endian.read2be(r, &len);
	printf("quant table lenchunnk = %d\n", len);
	uint8_t hdr;
	uint8_t *qt = calloc!(64, 1);
	reader.read(r, &hdr, 1);
	reader.read(r, qt, 64);
	self->quant[hdr & 0xf] = qt;
}

void read_baseline_dct(jpeg_t *self, reader.t *r) {
	uint16_t len = 0;
	endian.read2be(r, &len);
	printf("dct len = %d\n", len);

	uint8_t hdr;
	uint16_t h;
	uint16_t w;
	uint8_t components;
	reader.read(r, &hdr, 1);
	endian.read2be(r, &h);
	endian.read2be(r, &w);
	reader.read(r, &components, 1);

	printf("hdr=%u\n", hdr);
	printf("size %ux%u, %u components\n", w,  h, components);
	self->img = image.new(w, h);
	for (uint8_t i = 0; i < components; i++) {
		uint8_t id;
		uint8_t samp;
		uint8_t QtbId;
		reader.read(r, &id, 1);
		reader.read(r, &samp, 1);
		reader.read(r, &QtbId, 1);
		printf("id=%u samp=%u QtbId=%u\n", id, samp, QtbId);
		self->quantMapping[i] = QtbId;
	}
}

void read_huffman_table(jpeg_t *self, reader.t *r) {
	uint16_t len = 0;
	endian.read2be(r, &len);
	printf("huffman len = %d\n", len);

	uint8_t hdr;
	uint8_t lengths[16] = {};
	reader.read(r, &hdr, 1);
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
	self->htables[hdr] = huffman.treefrom(lengths, 16, elements);
	free(elements);
}

void read_scan(jpeg_t *self, reader.t *r) {
	uint16_t len = 0;
	endian.read2be(r, &len);
	printf("lenchunnk = %d\n", len);

	//
	// read scan header
	//
	reader.skip(r, len-2);

	reader.t *e = escreader(r);
	read_scan_data(self, e);
	reader.free(e);
}

void read_scan_data(jpeg_t *self, reader.t *r) {
	bits.reader_t *br = bits.newreader(r);

	int oldlumdccoeff = 0;
	int oldCbdccoeff = 0;
	int oldCrdccoeff = 0;

	int w = self->img->width;
	int h = self->img->height;
	for (int row = 0; row < h / 8; row++) {
		for (int col = 0; col < w / 8; col++) {
			//
			// Read 3 component blocks
			//
			int vals1[64] = {};
			int vals2[64] = {};
			int vals3[64] = {};


			int idx = 0;
			huffman.reader_t *hrdc = huffman.newreader(self->htables[0+idx], br);
			huffman.reader_t *hrac = huffman.newreader(self->htables[16+idx], br);
			readblock(br, hrdc, hrac, oldlumdccoeff, vals1);
			oldlumdccoeff = vals1[0];
			huffman.closereader(hrdc);
			huffman.closereader(hrac);

			idx = 1;
			hrdc = huffman.newreader(self->htables[0+idx], br);
			hrac = huffman.newreader(self->htables[16+idx], br);
			readblock(br, hrdc, hrac, oldCrdccoeff, vals2);
			oldCrdccoeff = vals2[0];
			huffman.closereader(hrdc);
			huffman.closereader(hrac);


			idx = 1;
			hrdc = huffman.newreader(self->htables[0+idx], br);
			hrac = huffman.newreader(self->htables[16+idx], br);
			readblock(br, hrdc, hrac, oldCbdccoeff, vals3);
			oldCbdccoeff = vals3[0];
			huffman.closereader(hrdc);
			huffman.closereader(hrac);

			//
			// Undo quantization
			//
			uint8_t *quant1 = self->quant[self->quantMapping[0]];
			uint8_t *quant2 = self->quant[self->quantMapping[1]];
			uint8_t *quant3 = self->quant[self->quantMapping[2]];
			for (int i = 0; i < 64; i++) {
				vals1[i] *= quant1[i];
				vals2[i] *= quant2[i];
				vals3[i] *= quant3[i];
			}

			//
			// Rebuild the components
			//
			int L[64] = {};
			for (int i = 0; i < 64; i++) {
				if (vals1[i] == 0) continue;
				AddZigZag(L, i, vals1[i]);
			}
			int Cr[64] = {};
			for (int i = 0; i < 64; i++) {
				if (vals2[i] == 0) continue;
				AddZigZag(Cr, i, vals2[i]);
			}
			int Cb[64] = {};
			for (int i = 0; i < 64; i++) {
				if (vals3[i] == 0) continue;
				AddZigZag(Cb, i, vals3[i]);
			}

			// store it as RGB
			for (int y = 0; y < 8; y++) {
				for (int x = 0; x < 8; x++) {
					int bpos = x + 8*y;
					image.rgba_t val = ColorConversion(L[bpos], Cb[bpos], Cr[bpos]);
					image.set(self->img, col*8 + x, row*8 + y, val);
				}
			}
		}
	}
}

void AddZigZag(int *base, int zi, double coeff) {
	uint8_t i = zigzag[zi];
	int n = i & 0x7;
	int m = i >> 3;
	double shape[64];
	getshape(shape, n, m);
	mmulk(shape, coeff);
	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 8; x++) {
			base[x + 8*y] += shape[x + 8*y];
		}
	}
}

void mmulk(double *m, double k) {
	for (int i = 0; i < 64; i++) {
		m[i] *= k;
	}
}

// void madd(double *s, double *m) {
// 	for (int i = 0; i < 64; i++) {
// 		s[i] += m[i];
// 	}
// }

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

uint8_t Clamp(double col) {
	if (col > 255) col = 255;
	if (col < 0) col = 0;
	return (uint8_t) col;
}

image.rgba_t ColorConversion(double Y, Cr, Cb) {
	image.rgba_t c = {};
	double R = Cr * (2 - 2 * 0.299) + Y;
	double B = Cb * (2 -2 * 0.114) + Y;
	double G = (Y - 0.114 * B - 0.299*R) / 0.587;
	c.red = Clamp(R+128);
	c.green = Clamp(G+128);
	c.blue = Clamp(B+128);
	return c;
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
	if (bval>=l) {
		return bval;
	}
	return bval-(2*l-1);
}

typedef {
    reader.t *in;
    bool ended;
} escaper_t;

reader.t *escreader(reader.t *in) {
	escaper_t *e = calloc!(1, sizeof(escaper_t));
	e->in = in;
	return reader.new(e, escreadn, free);
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
    if (r->ended) panic("reading from closed escaper");
    uint8_t x;
    reader.read(r->in, &x, 1);
    if (x == 0xff) {
        reader.read(r->in, &x, 1);
        // 0xff 0x00 means just 0xff as data.
        if (x == 0) {
            // printf("[0xff as data]");
            return 0xff;
        }
        // 0xff 0xd9 means end of data.
        if (x == 0xd9) {
            puts("end of data");
            r->ended = true;
            return -1;
        }
        panic("unexpected 0xff");
    }
    return x;
}
