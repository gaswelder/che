#import bitwriter
#import image

pub typedef	{
	uint8_t *data;
	int width;
	int height;
} t;

pub t *new(int width, height) {
	t* img = calloc(sizeof(t), 1);
	img->width = width;
	img->height = height;
	img->data = calloc(width * height * 3, 1);
	return img;
}

pub void set(t* img, int x, y, uint8_t r, g, b) {
	uint8_t *p = get_pixel(img, x, y);
	*p++ = b;
	*p++ = g;
	*p++ = r;
}

pub void free(t *img) {
	OS.free(img->data);
	OS.free(img);
}

pub void writeimg(image.image_t *img, const char *filename) {
	t *cmap = new(img->width, img->height);
	for (int x = 0; x < img->width; x++) {
		for (int y = 0; y < img->height; y++) {
			image.rgb_t *c = image.getpixel(img, x, y);
			set(cmap, x, y, c->red, c->green, c->blue);
		}
	}
	_write(cmap, filename);
	free(cmap);
}

bool _write(t* img, const char *filename) {
	FILE *out = fopen(filename, "w");
	if (!out) return false;

	bitwriter.t *w = calloc(1, sizeof(bitwriter.t));
	if (!w) {
		fclose(out);
		return false;
	}

	w->f = out;

	// Pixel rows have to be padded to the nearest multiple of 4 bytes.
	int remainder = (img->width * 3) % 4;
	int pad = (4 - remainder) % 4;

	// We have to know the file size in advance.
	// The file is header (14 bytes) + bitmapinfo (40 bytes) + data.
	// The data is rows (height) * row size (pixel size * width + pad).

	size_t headers_size = 14 + 40;
	size_t image_data_size = (img->height * (img->width * 3 + pad));
	size_t file_size = headers_size + image_data_size;

	// -- header --
	// 2 bytes magic number: 0x42 0x4d.
	bitwriter.byte(w, 0x42);
	bitwriter.byte(w, 0x4d);
	// file size in bytes
	bitwriter.le32(w, file_size);
	// reserved 4 bytes
	bitwriter.le32(w, 0);
	// image data offset
	bitwriter.le32(w, headers_size);

	// -- windows bitmap info --
	// 4 the size of this header, in bytes (40)
	bitwriter.le32(w, 40);
	// width and height, 4 bytes signed each.
	bitwriter.le32(w, img->width);
	bitwriter.le32(w, img->height);
	// 2 bytes, the number of color planes (must be 1).
	bitwriter.le16(w, 1);
	// 2 bytes, the number of bits per pixel.
	// Typical values are 1, 4, 8, 16, 24 and 32.
	bitwriter.le16(w, 24);
	// 4 bytes, the compression method being used.
	bitwriter.le32(w, 0);

	// 4 bytes, the size of the raw bitmap data; a dummy 0 can be given for BI_RGB bitmaps.
	bitwriter.le32(w, image_data_size);

	// horizontal and vertical resolution, pixel per metre, signed integer, 4 bytes each.
	bitwriter.le32(w, 0);
	bitwriter.le32(w, 0);

	// 4 bytes, the number of colors in the color palette. 0 to default to 2^n
	// 4 bytes, the number of important colors used, or 0 when every color is important; generally ignored 
	bitwriter.le32(w, 0);
	bitwriter.le32(w, 0);

	for (int y = img->height - 1; y >= 0; y--) {
		for (int x = 0; x < img->width; x++) {
			uint8_t *p = get_pixel(img, x, y);
			uint8_t b = *p++;
			uint8_t g = *p++;
			uint8_t r = *p++;
			bitwriter.byte(w, b);
			bitwriter.byte(w, g);
			bitwriter.byte(w, r);
		}
		for (int i = 0; i < pad; i++) bitwriter.byte(w, 0);
	}

	OS.free(w);
	return fclose(out) == 0;
}

uint8_t *get_pixel(t *img, int x, y) {
    size_t lw = img->width * 3;
    size_t pos = lw * y + x * 3;
    uint8_t *pixel = &img->data[pos];
    return pixel;
}
