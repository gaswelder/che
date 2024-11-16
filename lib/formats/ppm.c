// https://en.wikipedia.org/wiki/Netpbm_format

// Pipe the output into mpv

#import image

pub typedef { float r, g, b; } rgb_t;

pub typedef {
	rgb_t *frame;
	int width, height;
} ppm_t;

pub ppm_t *init(int width, height) {
	ppm_t *p = calloc(1, sizeof(ppm_t));
	p->width = width;
	p->height = height;
	p->frame = calloc(width * height, sizeof(rgb_t));
	return p;
}

pub void free(ppm_t *p) {
	OS.free(p->frame);
	OS.free(p);
}

/**
 * Sets pixel color at given frame coordinates.
 */
pub void set(ppm_t *p, int x, y, rgb_t color)
{
	p->frame[x + y * p->width] = color;
}

/**
 * Returns pixel color at given frame coordinates.
*/
pub rgb_t get(ppm_t *p, int x, y)
{
	return p->frame[x + p->width * y];
}

// Blends color with the current color at pixel (x, y).
pub void blend(ppm_t *p, int x, y, rgb_t color, float opacity) {
	rgb_t newcolor = get(p, x, y);
	newcolor.r = opacity * color.r + (opacity - 1) * newcolor.r;
	newcolor.g = opacity * color.g + (opacity - 1) * newcolor.g;
	newcolor.b = opacity * color.b + (opacity - 1) * newcolor.b;
	set(p, x, y, newcolor);
}

/**
 * Writes the current frame buffer to the given file and clears the frame
 * buffer.
 */
pub void write(ppm_t *p, FILE *f)
{
	fprintf(f, "P6\n%d %d\n255\n", p->width, p->height);
	int size = p->width * p->height;
	uint32_t buf = 0;
	for (int i = 0; i < size; i++) {
		buf = format_rgb(p->frame[i]);
		fwrite(&buf, 3, 1, f);
	}
}

pub void clear(ppm_t *p)
{
	memset(p->frame, 0, p->width * p->height * sizeof(rgb_t));
}

/* Convert RGB to 24-bit color. */
uint32_t format_rgb(rgb_t c)
{
	uint32_t ir = roundf(c.r * 255.0f);
	uint32_t ig = roundf(c.g * 255.0f);
	uint32_t ib = roundf(c.b * 255.0f);
	return (ir << 16) | (ig << 8) | ib;
}

pub void writeimg(image.image_t *img, FILE *f) {
	fprintf(f, "P6\n%d %d\n255\n", img->width, img->height);

	uint32_t buf = 0;
	for (int y = 0; y < img->height; y++) {
		for (int x = 0; x < img->width; x++) {
			image.rgb_t c0 = image.get(img, x, y);
			uint32_t ir = c0.red;
			uint32_t ig = c0.green;
			uint32_t ib = c0.blue;
			buf = (ir << 16) | (ig << 8) | ib;
			fwrite(&buf, 3, 1, f);
		}
	}
}
