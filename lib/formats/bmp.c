#import enc/endian
#import image
#import writer

// Writes the image img to the specified file.
pub bool write(image.image_t *img, const char *filename) {
	FILE *out = fopen(filename, "w");
	if (!out) {
		return false;
	}
	writer.t *w = writer.file(out);

	int width = img->width;
	int height = img->height;

	// Pixel rows have to be padded to the nearest multiple of 4 bytes.
	int remainder = (width * 3) % 4;
	int pad = (4 - remainder) % 4;

	// We have to know the file size in advance.
	// The file is header (14 bytes) + bitmapinfo (40 bytes) + data.
	// The data is rows (height) * row size (pixel size * width + pad).
	size_t headers_size = 14 + 40;
	size_t image_data_size = (height * (width * 3 + pad));
	size_t file_size = headers_size + image_data_size;

	// -- header --
	// 2 bytes magic number: 0x42 0x4d.
	endian.write1(w, 0x42);
	endian.write1(w, 0x4d);

	endian.write4le(w, file_size); // file size in bytes
	endian.write4le(w, 0); // reserved 4 bytes
	endian.write4le(w, headers_size); // image data offset

	// -- windows bitmap info --
	// size of this header, in bytes (40)
	endian.write4le(w, 40);
	// width and height, 4 bytes signed each.
	endian.write4le(w, width);
	endian.write4le(w, height);
	// number of color planes (must be 1).
	endian.write2le(w, 1);

	// 2 bytes, the number of bits per pixel.
	// Typical values are 1, 4, 8, 16, 24 and 32.
	endian.write2le(w, 24);

	// compression method
	endian.write4le(w, 0);

	// size of the raw bitmap data
	// dummy 0 can be given for BI_RGB bitmaps.
	endian.write4le(w, image_data_size);

	// horizontal and vertical resolution, pixel per metre, signed integer, 4 bytes each.
	endian.write4le(w, 0);
	endian.write4le(w, 0);

	// 4 bytes, the number of colors in the color palette. 0 to default to 2^n
	// 4 bytes, the number of important colors used, or 0 when every color is important; generally ignored
	endian.write4le(w, 0);
	endian.write4le(w, 0);

	for (int y = height - 1; y >= 0; y--) {
		for (int x = 0; x < width; x++) {
			image.rgba_t *c = image.getpixel(img, x, y);
			endian.write1(w, c->blue);
			endian.write1(w, c->green);
			endian.write1(w, c->red);
		}
		for (int i = 0; i < pad; i++) {
			endian.write1(w, 0);
		}
	}

	OS.free(w);
	return fclose(out) == 0;
}
