#import bits
#import compress/huffman
#import enc/endian
#import error
#import formats/tiff
#import image
#import reader

const char *component_ids[] = { "invalid (0)", "Y", "Cb", "Cr", "I", "Q" };

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
	int mcux; // MCU size in pixels, = 8*max(hsize)
	int mcuy; // = 8*max(vsize)
	uint8_t dc_table[3]; // SOS huffman table ids, indexed by component
	uint8_t ac_table[3];
	int orientation; // EXIF orientation (1..8), 1 = as stored
} jpeg_t;

pub jpeg_t *read(const char *path, error.t *err) {
	FILE *f = fopen(path, "rb");
	reader.t *R = reader.file(f);
	jpeg_t *self = calloc!(1, sizeof(jpeg_t));
	while (true) {
		uint16_t hdr = 0;
		endian.read2be(R, &hdr);
		if (hdr == 0) {
			break;
		}
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
	if (self->img && self->orientation > 1) {
		self->img = fix_orientation(self->img, self->orientation);
	}
	return self;
}

image.image_t *fix_orientation(image.image_t *img, int orient) {
	int w = img->width;
	int h = img->height;
	int nw = w;
	int nh = h;
	switch (orient) {
		case 5, 6, 7, 8: {
			nw = h;
			nh = w;
		}
	}
	image.image_t *r = image.new(nw, nh);
	for (int x = 0; x < nw; x++) {
		for (int y = 0; y < nh; y++) {
			int sx = x;
			int sy = y;
			switch (orient) {
				case 2: { sx = w - 1 - x; } // flip horizontal
				case 3: {
					sx = w - 1 - x;
					sy = h - 1 - y;
				} // 180
				case 4: { sy = h - 1 - y; } // flip vertical
				case 5: {
					sx = y;
					sy = x;
				} // transpose
				case 6: {
					sx = y;
					sy = h - 1 - x;
				} // 90 CW
				case 7: {
					sx = w - 1 - y;
					sy = h - 1 - x;
				} // transverse
				case 8: {
					sx = x;
					sy = w - 1 - y;
				} // 90 CCW
			}
			image.set(r, x, y, image.get(img, sx, sy));
		}
	}
	image.free(img);
	return r;
}

pub void free(jpeg_t *j) {
	OS.free(j);
}

void read_app1(jpeg_t *self, reader.t *r) {
	uint16_t len;
	endian.read2be(r, &len);
	printf("App1 (len=%u)\n", len);

	uint8_t *buf = calloc!(len - 2, 1);
	reader.read(r, buf, len - 2);

	if (strcmp((char *) buf, "Exif") == 0 && buf[4] == 0 && buf[5] == 0) {
		tiff.file_t *tf = tiff.parse(buf + 6, len-2-6);
		uint32_t gpspos = 0;
		int orientation = 1;
		for (size_t i = 0; i < tf->ndirs; i++) {
			tiff.dir_t *d = tf->dirs[i];
			for (size_t j = 0; j < d->nentries; j++) {
				if (d->entries[j]->tag == 34853) {
					gpspos = d->entries[j]->value;
					break;
				}
				if (d->entries[j]->tag == 274) {
					orientation = d->entries[j]->value;
				}
			}
		}
		self->orientation = orientation;
		if (gpspos != 0) {
			tiff.setpos(tf, gpspos);
			tiff.read_dir(tf);
		}
		for (size_t i = 0; i < tf->ndirs; i++) {
			tiff.dir_t *d = tf->dirs[i];
			dumpdir(d);
		}
	}
	OS.free(buf);
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
			default: { printf("%u (%zu)", e->value, e->count); }
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
	reader.skip(r, len - 2);
}

void read_app4(jpeg_t *self, reader.t *r) {
	(void) self;
	uint16_t len;
	endian.read2be(r, &len);
	printf("App4 (len=%u)\n", len);
	// ?
	reader.skip(r, len - 2);
}

void read_appdef(jpeg_t *self, reader.t *r) {
	(void) self;
	uint16_t len;
	endian.read2be(r, &len);
	printf("Application Default Header (len=%u)\n", len);
	reader.skip(r, len - 2);
}

void read_restart_interval(jpeg_t *self, reader.t *r, error.t *err) {
	uint16_t len;
	if (!endian.read2be(r, &len)) {
		error.set(err, "failed to read section length");
		return;
	}
	if (len != 4) {
		error.set(err, "restart interval: expected len=4, got %u", len);
		return;
	}
	if (!endian.read2be(r, &self->restart_interval)) {
		error.set(err, "failed to read restart interval");
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

void read_baseline_dct(jpeg_t *self, reader.t *r, error.t *err) {
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
	printf("\tsize = %u x %u\n", w, h);
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
		printf("\tcomponent %u: id=%u (%s)", i, id, component_ids[id]);
		printf(" sampling_factors=%d,%d qtable_num=%u\n", c->hsize, c->vsize, c->qtable_index);
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
	uint8_t maxh = 0;
	uint8_t maxv = 0;
	for (uint8_t i = 0; i < self->ncomponents; i++) {
		if (self->components[i].hsize > maxh) {
			maxh = self->components[i].hsize;
		}
		if (self->components[i].vsize > maxv) {
			maxv = self->components[i].vsize;
		}
	}
	self->mcux = 8 * maxh;
	self->mcuy = 8 * maxv;
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
	//	 case 0: { info->dc_huff_trees[tid] = t; }
	//	 case 1: { info->ac_huff_trees[tid] = t; }
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
		// Match the scan component id to a frame component index.
		for (uint8_t ci = 0; ci < self->ncomponents; ci++) {
			if (self->components[ci].id == id) {
				self->dc_table[ci] = dc_table_id;
				self->ac_table[ci] = ac_table_id;
				break;
			}
		}
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
	bits.reader_t *br = bits.newreader(r, bits.STRAIGHT);

	// First values ("DC") are diff-encoded across all blocks.
	// These will contain the current values.
	int dc[3] = { 0, 0, 0 };
	int w = self->img->width;
	int h = self->img->height;
	int mcus_w = (w + self->mcux - 1) / self->mcux;
	int mcus_h = (h + self->mcuy - 1) / self->mcuy;
	int ri = self->restart_interval;
	image.image_t *mcu = image.new(self->mcux, self->mcuy);
	int i = 0;
	for (int my = 0; my < mcus_h; my++) {
		for (int mx = 0; mx < mcus_w; mx++) {
			// Restart boundary: drop the remaining bits of the partial
			// byte, consume the restart marker, reset DC predictors.
			if (ri > 0 && i > 0 && i % ri == 0) {
				br->rem = 0;
				uint8_t m = 0;
				if (reader.read(r, &m, 1) != 1 || m < 0xd0 || m > 0xd7) {
					panic("restart marker expected, got %x", m);
				}
				dc[0] = 0;
				dc[1] = 0;
				dc[2] = 0;
			}
			read_mcu(self, dc, br, mcu);
			image.paste(self->img, mcu, mx * self->mcux, my * self->mcuy);
			i++;
		}
	}
	image.free(mcu);
	bits.closereader(br);
}

void read_mcu(jpeg_t *self, int *dc, bits.reader_t *br, image.image_t *mcu) {
	int ncomp = self->ncomponents;
	double planes[3][32][32] = {};

	for (int ci = 0; ci < ncomp; ci++) {
		component_t *c = &self->components[ci];
		uint8_t *quant = self->quant[c->qtable_index];
		int hb = c->hsize;
		int vb = c->vsize;

		huffman.reader_t *hrdc = huffman.newreader(self->htables[self->dc_table[ci]], br);
		huffman.reader_t *hrac = huffman.newreader(self->htables[16 + self->ac_table[ci]], br);

		for (int by = 0; by < vb; by++) {
			for (int bx = 0; bx < hb; bx++) {
				int vals[64] = {};
				readblock(br, hrdc, hrac, dc[ci], vals);
				dc[ci] = vals[0];

				// Undo quantization
				for (int j = 0; j < 64; j++) {
					vals[j] *= quant[j];
				}

				// Undo zigzag
				undozz(vals);

				// Rebuild the component values
				double block[64] = {};
				rebuild(vals, block);

				for (int j = 0; j < 64; j++) {
					int px = j & 0x7;
					int py = j >> 3;
					planes[ci][by * 8 + py][bx * 8 + px] = block[j];
				}
			}
		}
		huffman.closereader(hrdc);
		huffman.closereader(hrac);
	}

	// Component 0 (Y) spans the whole MCU. Chroma components are sampled
	// down by (mcux/(8*hsize), mcuy/(8*vsize)) and upsampled by replication.
	int hscale1 = self->mcux / (8 * self->components[1].hsize);
	int vscale1 = self->mcuy / (8 * self->components[1].vsize);
	int hscale2 = self->mcux / (8 * self->components[2].hsize);
	int vscale2 = self->mcuy / (8 * self->components[2].vsize);
	for (int y = 0; y < self->mcuy; y++) {
		for (int x = 0; x < self->mcux; x++) {
			double Y = planes[0][y][x];
			double Cb = planes[1][y / vscale1][x / hscale1];
			double Cr = planes[2][y / vscale2][x / hscale2];
			image.rgba_t col = toRGB(Y, Cr, Cb);
			image.set(mcu, x, y, col);
		}
	}
}

void rebuild(int *weights, double *out) {
	double shape[64];
	for (int i = 0; i < 64; i++) {
		int w = weights[i];
		if (w == 0) {
			continue;
		}
		int n = i & 0x7;
		int m = i >> 3;
		getshape(shape, n, m);
		mmulk(shape, (double) w);
		madd(out, shape);
	}
}

const uint8_t zigzag[] = {
	 0,  1,  8, 16,  9,  2,  3, 10,
	17, 24, 32, 25, 18, 11,  4,  5,
	12, 19, 26, 33, 40, 48, 41, 34,
	27, 20, 13,  6,  7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36,
	29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46,
	53, 60, 61, 54, 47, 55, 62, 63
};

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
	if (n == 0) {
		a = sqrt(0.5);
	}
	if (m == 0) {
		b = sqrt(0.5);
	}
	double ka = n * M_PI / 16.0;
	double kb = m * M_PI / 16.0;
	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 8; x++) {
			double nn = a * cos(ka * (2 * x + 1.0));
			double mm = b * cos(kb * (2 * y + 1.0));
			shape[x + 8 * y] = 0.25 * nn * mm;
		}
	}
}

image.rgba_t toRGB(double Y, Cr, Cb) {
	image.rgba_t c = {};
	double R = Cr * (2 - 2 * 0.299) + Y;
	double B = Cb * (2 - 2 * 0.114) + Y;
	double G = (Y - 0.114 * B - 0.299 * R) / 0.587;
	c.red = clamp(R + 128);
	c.green = clamp(G + 128);
	c.blue = clamp(B + 128);
	return c;
}

uint8_t clamp(double x) {
	if (x > 255) {
		return 255;
	}
	if (x < 0) {
		return 0;
	}
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
		if (code == EOF) {
			panic("eof");
		}
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
		if (l < 64) {
			int coeff = weirdonum(br, code);
			vals[l] = coeff;
			l += 1;
		}
	}
}

int weirdonum(bits.reader_t *br, int code) {
	int bval = bits.readn(br, code);
	int l = 1 << (code - 1);
	if (bval >= l) {
		return bval;
	}
	int z = 2 * l - 1;
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
		if (c == -1) {
			return -1;
		}
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
		// 0xff 0xd0..0xd7 is a restart marker.
		if (x >= 0xd0 && x <= 0xd7) {
			return x;
		}
		panic("unexpected 0xff 0x%x", x);
	}
	return x;
}
