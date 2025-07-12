#import image

// Determines how the pixels are stored.
pub enum {
	PNG_GRAYSCALE = 0, // 256 shades of gray, 8 bits per pixel
	PNG_RGB = 2, // 24-bit RGB values
	PNG_PALETTE = 3, // Up to 256 RGBA colors, 8 bits per pixel
	PNG_GRAYSCALE_ALPHA = 4, // 256 shades of gray plus alpha channel, 16 bits per pixel
	PNG_RGBA = 6 // 24-bit RGB values plus 8-bit alpha channel
}

typedef {
	uint32_t crc; // crc register
	uint16_t s1, s2; // adler registers
	FILE *f; // output file
} writer_t;

pub bool write(image.image_t *img, const char *path, int mode) {
	writer_t *tmp = calloc!(sizeof(writer_t), 1);
	tmp->f = fopen(path, "wb");
	if (!tmp->f) {
		panic("failed to open output file '%s'", path);
	}

	size_t width = img->width;
	size_t height = img->height;


	//
	// Initialize the palette.
	//
	uint32_t palette[256] = {};
	size_t psize = 0;
	if (mode == PNG_PALETTE) {
		for (size_t j=0; j < height; j++) {
			for (size_t i=0; i < width; i++) {
				image.rgba_t c = image.get(img, i, j);
				uint32_t encoded = pack32(c);

				int index = indexof(palette, psize, encoded);
				if (index < 0) {
					if (psize < 256) {
						index = psize;
						palette[psize++] = encoded;
					} else {
						index = psize - 1;
					}
				}
			}
		}
	}



	png_out_raw_write(tmp, "\211PNG\r\n\032\n", 8);

	/* IHDR */
	png_new_chunk(tmp, "IHDR", 13);
	png_out_uint32(tmp, png_swap32((uint32_t) (width)));
	png_out_uint32(tmp, png_swap32((uint32_t) (height)));
	write_byte_crc(tmp, 8); /* bit depth */
	write_byte_crc(tmp, (uint8_t) mode);
	write_byte_crc(tmp, 0); /* compression */
	write_byte_crc(tmp, 0); /* filter */
	write_byte_crc(tmp, 0); /* interlace method */
	png_end_chunk(tmp);

	//
	// Write out the palette, if in palette mode.
	//
	if (mode == PNG_PALETTE) {
		char entry[3];
		size_t s = psize;
		if (s < 16) {
			s = 16; /* minimum palette length */
		}
		png_new_chunk(tmp, "PLTE", 3 * s);
		for (size_t index = 0; index < s; index++) {
			uint32_t color = palette[index];
			// ? | ? | ?
			entry[0] = (char) (color & 255);
			entry[1] = (char) ((color >> 8) & 255);
			entry[2] = (char) ((color >> 16) & 255);
			png_out_write(tmp, entry, 3);
		}
		png_end_chunk(tmp);

		/* transparency */
		png_new_chunk(tmp, "tRNS", s);
		for (size_t index = 0; index < s; index++) {
			entry[0] = (char) ((palette[index] >> 24) & 255);
			png_out_write(tmp, entry, 1);
		}
		png_end_chunk(tmp);
	}

	//
	// Write out data header
	//
	size_t bpp = 1;
	if (mode == PNG_PALETTE) {
		bpp = 1;
	} else if (mode == PNG_GRAYSCALE) {
		bpp = 1;
	} else if (mode == PNG_GRAYSCALE_ALPHA) {
		bpp = 2;
	} else if (mode == PNG_RGB) {
		bpp = 3;
	} else if (mode == PNG_RGBA) {
		bpp = 4;
	}
	size_t bpl = 1 + bpp * width;
	if (bpl >= 65536) {
		panic("[tmp] ERROR: maximum supported width for this type of PNG is %d pixel\n", (int)(65535 / bpp));
	}
	size_t size = 2 + height * (5 + bpl) + 4;
	png_new_chunk(tmp, "IDAT", size);
	png_out_write(tmp, "\170\332", 2);

	//
	// Write out the pixels
	//
	tmp->s1 = 1;
	tmp->s2 = 0;
	for (size_t y = 0; y < height; y++) {
		// Line header
		if (y < height - 1) {
			png_out_write(tmp, "\0", 1);
			png_out_uint16(tmp, (uint16_t) bpl);
			png_out_uint16(tmp, (uint16_t) ~bpl);
		} else {
			/* last line */
			png_out_write(tmp, "\1", 1);
			png_out_uint16(tmp, (uint16_t) bpl);
			png_out_uint16(tmp, (uint16_t) ~bpl);
		}
		png_out_write_adler(tmp, 0); /* no filter */

		for (size_t x = 0; x < width; x++) {
			image.rgba_t c = image.get(img, x, y);
			switch (mode) {
				case PNG_PALETTE: {
					png_out_write_adler(tmp, (uint8_t) indexof(palette, psize, pack32(c)));
				}
				case PNG_RGB: {
					png_out_write_adler(tmp, c.red);
					png_out_write_adler(tmp, c.green);
					png_out_write_adler(tmp, c.blue);
				}
				case PNG_GRAYSCALE: {
					uint8_t color = (uint8_t) (c.red * 0.2989 + c.green * 0.5870 + c.blue * 0.1140);
					png_out_write_adler(tmp, color);
				}
				case PNG_GRAYSCALE_ALPHA: {
					int g = (uint8_t) (c.red * 0.2989 + c.green * 0.5870 + c.blue * 0.1140);
					png_out_write_adler(tmp, g);
					png_out_write_adler(tmp, c.transparency);
				}
				case PNG_RGBA: {
					png_out_write_adler(tmp, c.red);
					png_out_write_adler(tmp, c.green);
					png_out_write_adler(tmp, c.blue);
					png_out_write_adler(tmp, c.transparency);
				}
				default: {
					panic("unknown mode");
				}
			}
		}
	}

	//
	// Checksum
	//
	tmp->s1 %= ADLER_BASE;
	tmp->s2 %= ADLER_BASE;
	png_out_uint32(tmp, png_swap32((uint32_t) ((tmp->s2 << 16) | tmp->s1)));
	png_end_chunk(tmp);

	//
	// Image end
	//
	png_new_chunk(tmp, "IEND", 0);
	png_end_chunk(tmp);

	fclose(tmp->f);

	free(tmp);
	return true;
}

uint32_t pack32(image.rgba_t c) {
	return c.red | (c.green << 8) | (c.blue << 16) | (c.transparency << 24);
}

int indexof(uint32_t *xs, size_t len, uint32_t y) {
	for (size_t i = 0; i < len; i++) {
		if (xs[i] == y) {
			return i;
		}
	}
	return -1;
}


const int ADLER_BASE = 65521;

const uint32_t crc32[256] = {
		0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3, 0x0edb8832,
		0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
		0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856, 0x646ba8c0, 0xfd62f97a,
		0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
		0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3,
		0x45df5c75, 0xdcd60dcf, 0xabd13d59, 0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
		0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab,
		0xb6662d3d, 0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
		0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01, 0x6b6b51f4,
		0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
		0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65, 0x4db26158, 0x3ab551ce, 0xa3bc0074,
		0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
		0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525,
		0x206f85b3, 0xb966d409, 0xce61e49f, 0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
		0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615,
		0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
		0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7, 0xfed41b76,
		0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
		0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b, 0xd80d2bda, 0xaf0a1b4c, 0x36034af6,
		0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
		0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7,
		0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d, 0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
		0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7,
		0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
		0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45, 0xa00ae278,
		0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
		0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9, 0xbdbdf21c, 0xcabac28a, 0x53b39330,
		0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
		0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};



uint32_t png_swap32(uint32_t num) {
	return ((num >> 24) & 0xff) |
		   ((num << 8) & 0xff0000) |
		   ((num >> 8) & 0xff00) |
		   ((num << 24) & 0xff000000);
}

uint32_t png_crc(uint8_t *data, size_t len, uint32_t crc) {
	for (size_t i = 0; i < len; i++) {
		crc = crc32[(crc ^ data[i]) & 255] ^ (crc >> 8);
	}
	return crc;
}


void png_new_chunk(writer_t *png, const char *name, size_t len) {
	png->crc = 0xffffffff;
	png_out_raw_uint(png, png_swap32((uint32_t) len));
	png->crc = png_crc((uint8_t *) name, 4, png->crc);
	png_out_raw_write(png, name, 4);
}


void png_end_chunk(writer_t *png) {
	png_out_raw_uint(png, png_swap32(~png->crc));
}


void png_out_uint32(writer_t *png, uint32_t val) {
	png->crc = png_crc((uint8_t *) &val, 4, png->crc);
	png_out_raw_uint(png, val);
}


void png_out_uint16(writer_t *png, uint16_t val) {
	write_byte_crc(png, val % 256);
	val /= 256;
	write_byte_crc(png, val % 256);
	val /= 256;
}




void png_out_write(writer_t *png, const char *data, size_t len) {
	png->crc = png_crc((uint8_t *) data, len, png->crc);
	png_out_raw_write(png, data, len);
}


void png_out_write_adler(writer_t *png, uint8_t data) {
	png_out_write(png, (char *) &data, 1);
	png->s1 = (uint16_t) ((png->s1 + data) % ADLER_BASE);
	png->s2 = (uint16_t) ((png->s2 + png->s1) % ADLER_BASE);
}

void write_byte(writer_t *png, uint8_t val) {
	fputc(val, png->f);
}

void write_byte_crc(writer_t *png, uint8_t val) {
	png->crc = png_crc((uint8_t *) &val, 1, png->crc);
	write_byte(png, val);
}

void png_out_raw_uint(writer_t *png, uint32_t val) {
	write_byte(png, val % 256);
	val /= 256;
	write_byte(png, val % 256);
	val /= 256;
	write_byte(png, val % 256);
	val /= 256;
	write_byte(png, val % 256);
	val /= 256;
}

void png_out_raw_write(writer_t *png, const char *data, size_t len) {
	for (size_t i = 0; i < len; i++) {
		write_byte(png, data[i]);
	}
}




// https://github.com/misc0110/libattopng
// Copyright (c) 2018 Michael Schwarz and others

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
