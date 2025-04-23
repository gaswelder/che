// https://en.wikipedia.org/wiki/Netpbm_format

#import image

pub void writeimg(image.image_t *img, FILE *f) {
	fprintf(f, "P6\n%d %d\n255\n", img->width, img->height);
	uint32_t buf = 0;
	for (int y = 0; y < img->height; y++) {
		for (int x = 0; x < img->width; x++) {
			image.rgba_t c0 = image.get(img, x, y);
			uint32_t ir = c0.red;
			uint32_t ig = c0.green;
			uint32_t ib = c0.blue;
			buf = (ir << 16) | (ig << 8) | ib;
			fwrite(&buf, 3, 1, f);
		}
	}
}
