#import checksum/adler32
#import checksum/crc32
#import image

// Determines how the pixels are stored.
pub enum {
	PNG_GRAYSCALE = 0,       // 256 shades of gray, 8 bits per pixel
	PNG_RGB = 2,             // 24-bit RGB values
	PNG_PALETTE = 3,         // Up to 256 RGBA colors, 8 bits per pixel
	PNG_GRAYSCALE_ALPHA = 4, // 256 shades of gray plus alpha channel, 16 bits per pixel
	PNG_RGBA = 6,            // 24-bit RGB values plus 8-bit alpha channel
}

typedef {
	uint32_t crc; // CRC-32 of the current chunk
	uint32_t adler; // Adler-32 of the IDAT payload
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
		for (size_t j = 0; j < height; j++) {
			for (size_t i = 0; i < width; i++) {
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

	// IHDR
	png_new_chunk(tmp, "IHDR", 13);
	png_out_uint32(tmp, png_swap32((uint32_t) width));
	png_out_uint32(tmp, png_swap32((uint32_t) height));
	write_byte_crc(tmp, 8); // bit depth
	write_byte_crc(tmp, (uint8_t) mode);
	write_byte_crc(tmp, 0); // compression
	write_byte_crc(tmp, 0); // filter
	write_byte_crc(tmp, 0); // interlace method
	png_end_chunk(tmp);

	//
	// Write out the palette, if in palette mode.
	//
	if (mode == PNG_PALETTE) {
		char entry[3] = {};
		size_t s = max(16, psize); // 16 is the minimum palette length.
		png_new_chunk(tmp, "PLTE", 3 * s);
		for (size_t index = 0; index < s; index++) {
			uint32_t color = palette[index];
			// ? | ? | ?
			entry[0] = (char)(color & 255);
			entry[1] = (char)((color >> 8) & 255);
			entry[2] = (char)((color >> 16) & 255);
			png_out_write(tmp, entry, 3);
		}
		png_end_chunk(tmp);

		// transparency
		png_new_chunk(tmp, "tRNS", s);
		for (size_t index = 0; index < s; index++) {
			entry[0] = (char)((palette[index] >> 24) & 255);
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
	tmp->adler = 1;
	uint8_t *row = calloc!(bpl, 1);
	for (size_t y = 0; y < height; y++) {
		// Line header
		if (y < height - 1) {
			png_out_write(tmp, "\0", 1);
			png_out_uint16(tmp, (uint16_t) bpl);
			png_out_uint16(tmp, (uint16_t) ~bpl);
		} else {
			// last line
			png_out_write(tmp, "\1", 1);
			png_out_uint16(tmp, (uint16_t) bpl);
			png_out_uint16(tmp, (uint16_t) ~bpl);
		}
		row[0] = 0; // no filter
		size_t pos = 1;

		for (size_t x = 0; x < width; x++) {
			image.rgba_t c = image.get(img, x, y);
			switch (mode) {
				case PNG_PALETTE: {
					row[pos++] = (uint8_t) indexof(palette, psize, pack32(c));
				}
				case PNG_RGB: {
					row[pos++] = (uint8_t) c.red;
					row[pos++] = (uint8_t) c.green;
					row[pos++] = (uint8_t) c.blue;
				}
				case PNG_GRAYSCALE: {
					row[pos++] = (uint8_t)(c.red * 0.2989 + c.green * 0.5870 + c.blue * 0.1140);
				}
				case PNG_GRAYSCALE_ALPHA: {
					row[pos++] = (uint8_t)(c.red * 0.2989 + c.green * 0.5870 + c.blue * 0.1140);
					row[pos++] = (uint8_t) c.transparency;
				}
				case PNG_RGBA: {
					row[pos++] = (uint8_t) c.red;
					row[pos++] = (uint8_t) c.green;
					row[pos++] = (uint8_t) c.blue;
					row[pos++] = (uint8_t) c.transparency;
				}
				default: { panic("unknown mode"); }
			}
		}
		png_out_write(tmp, (char *) row, bpl);
		tmp->adler = adler32.update(tmp->adler, row, bpl);
	}
	free(row);

	png_out_uint32(tmp, png_swap32(tmp->adler));
	png_end_chunk(tmp);

	// Image end.
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

uint32_t png_swap32(uint32_t num) {
	return ((num >> 24) & 0xff) |
		   ((num << 8) & 0xff0000) |
		   ((num >> 8) & 0xff00) |
		   ((num << 24) & 0xff000000);
}

void png_new_chunk(writer_t *png, const char *name, size_t len) {
	png->crc = 0;
	png_out_raw_uint(png, png_swap32((uint32_t) len));
	png->crc = crc32.update(png->crc, (uint8_t *) name, 4);
	png_out_raw_write(png, name, 4);
}

void png_end_chunk(writer_t *png) {
	png_out_raw_uint(png, png_swap32(png->crc));
}

void png_out_uint32(writer_t *png, uint32_t val) {
	png->crc = crc32.update(png->crc, (uint8_t *) &val, 4);
	png_out_raw_uint(png, val);
}

void png_out_uint16(writer_t *png, uint16_t val) {
	write_byte_crc(png, val % 256);
	val /= 256;
	write_byte_crc(png, val % 256);
	val /= 256;
}

void png_out_write(writer_t *png, const char *data, size_t len) {
	png->crc = crc32.update(png->crc, (uint8_t *) data, len);
	png_out_raw_write(png, data, len);
}

void write_byte(writer_t *png, uint8_t val) {
	fputc(val, png->f);
}

void write_byte_crc(writer_t *png, uint8_t val) {
	png->crc = crc32.update(png->crc, &val, 1);
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
