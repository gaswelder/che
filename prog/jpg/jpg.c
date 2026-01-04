#import formats/jpg
#import formats/png
#import image
#import os/fs
#import compress/huffman
#import reader
#import bits

int main() {
    if (true) {
		testrun();
	} else {
		drawbasis();
	}
	return 0;
}

void testrun() {
	size_t fsize;
	uint8_t *raw = (uint8_t *) fs.readfile("0tmp.jpg", &fsize);

	slice_t *data = newslice();
	data->data = raw;
	data->len = (int) fsize;
	jpeg_t *self = jpeg();
	while (true) {
		uint16_t hdr = word(data->data[0], data->data[1]);
		printf("hdr = %x\n", hdr);
		uint16_t lenchunk = 0;
		if (hdr == 0xffd8) {
			lenchunk = 2;
		} else if (hdr == 0xffd9) {
			break;
		} else {
			lenchunk = word(data->data[2], data->data[3]);
			lenchunk+=2;
			printf("lenchunnk = %d\n", lenchunk);
			slice_t *chunk = slice(data, 4, lenchunk);
			if (hdr == 0xffdb) {
				DefineQuantizationTables(self, chunk->data, chunk->len);
			}
			else if (hdr == 0xffc0) {
				BaselineDCT(self, chunk);
			}
			else if (hdr == 0xffc4) {
				DefineHuffmanTables(self, chunk);
			}
			else if (hdr == 0xffda) {
				lenchunk = StartOfScan(self, data, lenchunk);
			}
		}
		data = slice(data, lenchunk, EOF);
		if (data->len == 0) break;
	}

	image.image_t *img = image.new(self->width, self->height);
	for (int x = 0; x < self->width; x++) {
		for (int y = 0; y < self->height; y++) {
			image.rgba_t c = self->image[y*self->width + x];
			// printf("%d %d %d\n", c.red, c.green, c.blue);
			image.set(img, x, y, c);
		}
	}
	png.write(img, "1.png", png.PNG_RGB);
}

// ---------------------------------------

typedef {
	uint8_t *data;
	int len;
} slice_t;

slice_t *newslice() {
	slice_t *s = calloc!(1, sizeof(slice_t));
	s->data = calloc!(100000, 1);
	return s;
}

slice_t *slice(slice_t *s, int start, int end) {
	slice_t *r = newslice();
	if (end == EOF) {
		end = s->len;
	}
	for (int i = start; i < end; i++) {
		append(r, s->data[i]);
	}
	return r;
}

void append(slice_t *s, uint8_t b) {
	s->data[s->len++] = b;
}

// -----------------------------------------

const double pi = 3.14157;

const uint8_t zigzag[] = {
	0,  1,  8, 16,  9,  2,  3, 10,
	17, 24, 32, 25, 18, 11,  4,  5,
	12, 19,	26, 33, 40, 48, 41, 34,
	27, 20, 13,  6,  7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36,
	29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46,
	53, 60, 61, 54, 47, 55, 62, 63};

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

// -----------------------------------------------

int DecodeNumber(int code, bits) {
	int l = 1 << (code-1);
	if (bits>=l) {
		return bits;
	}
	return bits-(2*l-1);
}

int XYtoLin(int x,y) { return x+y*8; }



int RemoveFF00(slice_t *data, *datapro) {
	if (!data || !datapro) panic("!");
	int i = 0;
	while (true) {
		uint8_t b = data->data[i];
		uint8_t bnext = data->data[i+1];
		if (b == 0xff) {
			if (bnext != 0) break;
			append(datapro, data->data[i]);
			i+=2;
		} else {
			append(datapro, data->data[i]);
			i+=1;
		}
	}
	return i;
}


double NormCoeff(int n) {
	if (n == 0) return sqrt( 1.0/8.0);
	return sqrt( 2.0/8.0);
}

typedef {
	int base[64];
} idct_t;

void AddIDC(idct_t *self, int n,m, double coeff) {
	double an = NormCoeff(n);
	double am = NormCoeff(m);
	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 8; x++) {
			double nn = an * cos( n* pi * (x + 0.5) / 8.0 );
			double mm = am * cos( m* pi * (y + 0.5) / 8.0 );
			self->base[ XYtoLin(x, y) ] += nn*mm*coeff;
		}
	}
}

void AddZigZag(idct_t *self, int zi, double coeff) {
	uint8_t i = zigzag[zi];
	int n = i & 0x7;
	int m = i >> 3;
	AddIDC(self, n,m, coeff);
}

uint16_t word(int a, b) {
	return a * 256 + b;
}







typedef {
	int width, height;
	image.rgba_t *image;
	huffman.tree_t *htables[100];
	uint8_t *quant[100];
	int quantMapping[100];
} jpeg_t;

jpeg_t *jpeg() {
	jpeg_t *self = calloc!(1, sizeof(jpeg_t));
	// self->quant = {}
	// self->quantMapping = []
	self->image = calloc!(100000, sizeof(image.rgba_t));
	return self;
}

idct_t *IDCT() {
	idct_t *self = calloc!(1, sizeof(idct_t));
	return self;
}

idct_t *BuildMatrix(jpeg_t *self, bits.reader_t *br, int idx, uint8_t *quant, int *_olddccoeff) {
	int olddccoeff = *_olddccoeff;
	idct_t *i = IDCT();

	huffman.tree_t *t = self->htables[0+idx];
	huffman.reader_t *hr = huffman.newreader(t, br);
	int code = huffman.read(hr);
	huffman.closereader(hr);

	int bval = bits.readn(br, code);
	int dccoeff = DecodeNumber(code, bval)  + olddccoeff;

	AddZigZag(i, 0,(dccoeff) * quant[0]);
	int l = 1;
	while (l<64) {
		huffman.tree_t *t = self->htables[16+idx];
		huffman.reader_t *hr = huffman.newreader(t, br);
		code = huffman.read(hr);
		huffman.closereader(hr);
		if (code == 0) {
			break;
		}
		if (code >15) {
			l+= (code>>4);
			code = code & 0xf;
		}
		bval = bits.readn(br, code);
		if (l<64) {
			int coeff = DecodeNumber(code, bval);
			AddZigZag(i, l,coeff * quant[l]);
			l+=1;
		}
	}
	*_olddccoeff = dccoeff;
	return i;
}

void dumpslice(slice_t *s) {
	printf("slice -------------\n");
	for (int i = 0; i < s->len; i++) {
		printf(" %2x", s->data[i]);
		if ((i + 1) % 16 == 0) printf("\n");
	}
	printf("-----------------\n");
}

int StartOfScan(jpeg_t *self, slice_t *data0, int hdrlen) {
	(void) dumpslice;
	slice_t *data = newslice();
	int lenchunk = RemoveFF00(slice(data0, hdrlen, EOF), data);

	bits.reader_t *br = bits.newreader(reader.static_buffer(data->data, data->len));

	int oldlumdccoeff = 0;
	int oldCbdccoeff = 0;
	int oldCrdccoeff = 0;

	for (int y = 0; y < self->height /8; y++) {
		for (int x = 0; x < self->width /8; x++) {
			idct_t *matL = BuildMatrix(self, br, 0, self->quant[self->quantMapping[0]], &oldlumdccoeff);
			idct_t *matCr = BuildMatrix(self, br, 1, self->quant[self->quantMapping[1]], &oldCrdccoeff);
			idct_t *matCb = BuildMatrix(self, br, 1, self->quant[self->quantMapping[2]], &oldCbdccoeff);

			// store it as RGB
			for (int yy = 0; yy < 8; yy++) {
				for (int xx = 0; xx < 8; xx++) {
					image.rgba_t val = ColorConversion(
						matL->base[XYtoLin(xx,yy)],
						matCb->base[XYtoLin(xx,yy)],
						matCr->base[XYtoLin(xx,yy)]
					);
					int pos = (x*8+xx) + ((y*8+yy) * self->width);
					self->image[pos] = val;
				}
			}
		}
	}
	return lenchunk + hdrlen;
}

void DefineQuantizationTables(jpeg_t *self, uint8_t *data, size_t len) {
	size_t off = 0;
	while (off < len) {
		uint8_t hdr = data[off++];
		uint8_t *qt = calloc!(64, 1);
		for (int i = 0; i < 64; i++) {
			qt[i] = data[off++];
		}
		self->quant[hdr & 0xf] = qt;
	}
}

void BaselineDCT(jpeg_t *self, slice_t *data) {
	uint8_t hdr = data->data[0];
	printf("hdr=%u\n", hdr);
	self->height = word(data->data[1], data->data[2]);
	self->width = word(data->data[3], data->data[4]);
	uint8_t components = data->data[5];
	printf("size %dx%d, %u components\n", self->width,  self->height, components);
	self->image = calloc!(1000000, 4);

	for (uint8_t i = 0; i < components; i++) {
		int pos = 6+i*3;
		uint8_t id = data->data[pos++];
		uint8_t samp = data->data[pos++];
		uint8_t QtbId = data->data[pos++];
		printf("id=%u samp=%u QtbId=%u\n", id, samp, QtbId);
		self->quantMapping[i] = QtbId;
	}
}

void DefineHuffmanTables(jpeg_t *self, slice_t *data) {
	printf("huffman\n");
	while (data->len > 0) {
		int off = 0;
		uint8_t hdr = data->data[off++];

		uint8_t *lengths = calloc!(16, 1);
		for (int i = 0; i < 16; i++) {
			lengths[i] = data->data[off++];
		}

		slice_t *elements = newslice();
		for (int a = 0; a < 16; a++) {
			uint8_t i = lengths[a];

			uint8_t *arr = calloc!(i, 1);
			for (uint8_t qq = 0; qq < i; qq++) {
				arr[qq] = data->data[off + (int) qq];
			}
			for (uint8_t qq = 0; qq < i; qq++) {
				append(elements, arr[qq]);
			}
			off += (int) i;
		}
		self->htables[hdr] = huffman.treefrom(lengths, 16, elements->data);
		data = slice(data, off, EOF);
	}
}


// -------------------------------

void drawbasis() {
	image.image_t *img = image.new(64 + 9, 64 + 9);
    image.rgba_t red = {255, 0, 0, 0};
    image.fill(img, red);
    for (int u = 0; u < 8; u++) {
        for (int v = 0; v < 8; v++) {
            square(img, u, v);
        }
    }
    image.image_t *img2 = image.explode(img, 10);
    image.free(img);

    png.write(img2, "1.png", png.PNG_RGB);
    image.free(img2);
}

void square(image.image_t *img, int u, v) {
    int x0 = 1;
    for (int i = 0; i < u; i++) {
        x0 += 1;
        x0 += 8;
    }
    int y0 = 1;
    for (int i = 0; i < v; i++) {
        y0 += 1;
        y0 += 8;
    }
    double shape[64];
    jpg.getshape(u, v, shape);
    for (int x = 0; x < 8; x++) {
        for (int y = 0; y < 8; y++) {
            double val = shape[x*8+y];
            val += 1;
            val /= 2; // 0..1
            image.set(img, x + x0, y + y0, image.gray((int) (val * 255)));
        }
    }
}
