pub typedef { int w, h; } dim_t;

pub typedef { int red, green, blue, transparency; } rgba_t;

pub typedef {
	int width;
	int height;
	rgba_t *data;
} image_t;

pub typedef void pixelfunc_t(rgba_t *);

// Creates a new image with the given dimensions.
pub image_t *new(int width, height) {
	image_t *img = calloc!(1, sizeof(image_t));
	img->width = width;
	img->height = height;
	img->data = calloc!(width * height, sizeof(rgba_t));
	return img;
}

// Frees the given image.
pub void free(image_t *img) {
	OS.free(img->data);
	OS.free(img);
}

pub rgba_t *getpixel(image_t *img, int x, y) {
	return &img->data[y * img->width + x];
}

// Sets the pixel at (x, y) to the given color.
pub void set(image_t *img, int x, y, rgba_t c) {
	checkcoords(img, x, y);
	*getpixel(img, x, y) = c;
}

// Returns the color at the given pixel.
pub rgba_t get(image_t *img, int x, y) {
	checkcoords(img, x, y);
	return *getpixel(img, x, y);
}

void checkcoords(image_t *img, int x, y) {
	if (x < 0 || y < 0) {
		panic("invalid pixel coordinates: %d, %d", x, y);
	}
	if (x >= img->width || y >= img->height) {
		panic("invalid pixel coordinates: %d, %d (should be less than %d, %d)", x, y, img->width, img->height);
	}
}

// Clears the entire image to all black.
pub void clear(image_t *img) {
	memset(img->data, 0, img->width * img->height * sizeof(rgba_t));
}

// Fills the whole image with the given color.
pub void fill(image_t *img, rgba_t color) {
	for (int x = 0; x < img->width; x++) {
		for (int y = 0; y < img->height; y++) {
			*getpixel(img, x, y) = color;
		}
	}
}

// Blends color with the current color at pixel (x, y).
pub void blend(image_t *img, int x, y, rgba_t color, float opacity) {
    rgba_t newcol = get(img, x, y);
    newcol.red = blendcolor(newcol.red, color.red, opacity);
	newcol.green = blendcolor(newcol.green, color.green, opacity);
	newcol.blue = blendcolor(newcol.blue, color.blue, opacity);
    set(img, x, y, newcol);
}

int blendcolor(int old, new, float opacity) {
	float oldpart = (opacity-1) * (float) old;
	float newpart = opacity * (float) new;
	return oldpart + newpart;
}

// Applies f to every pixel of the image.
pub void apply(image_t *img, pixelfunc_t f) {
	for (int y = 0; y < img->height; y++) {
		for (int x = 0; x < img->width; x++) {
			rgba_t color = get(img, x, y);
			f(&color);
			set(img, x, y, color);
		}
	}
}

// Creates a copy of img enlarged n times by duplicating the original pixels.
pub image_t *explode(image_t *img, int n) {
	image_t *big = new(img->width * n, img->height * n);
	int bigy = 0;
	for (int y = 0; y < img->height; y++) {
		// repeat row n times
		for (int i = 0; i < n; i++) {
			int bigx = 0;
			for (int x = 0; x < img->width; x++) {
				// repeat pixel n times
				rgba_t c = get(img, x, y);
				for (int j = 0; j < n; j++) {
					set(big, bigx, bigy, c);
					bigx++;
				}
			}
			bigy++;
		}
	}
	return big;
}

pub rgba_t white() {
	rgba_t c = {255, 255, 255, 0};
	return c;
}

pub rgba_t gray(int val) {
	rgba_t c = {val, val, val, 0};
	return c;
}

// Mixes colors a and b in the given proportion.
// Proportion 0 gives a, 1 gives b, values in between give a linear mix.
pub rgba_t mix(rgba_t a, b, double proportion_b) {
	double proportion_a = 1 - proportion_b;
	rgba_t c = {
		.red = (int) ( (double) a.red * proportion_a + (double) b.red * proportion_b ),
		.green = (int) ( (double) a.green * proportion_a + (double) b.green * proportion_b ),
		.blue = (int) ( (double) a.blue * proportion_a + (double) b.blue * proportion_b ),
	};
	return c;
}

pub rgba_t from_hsl(float h, s, l) {
	float c = (1 - fabs(2*l-1)) * s;
	float h1 = h / 60;
	float x = c * (1 - fabs(fmod(h1, 2)-1));
	float red;
	float green;
	float blue;
	if (h1 <= 1) {
		red = c;
		green = x;
		blue = 0;
	} else if (h1 <= 2) {
		red = x;
		green = c;
		blue = 0;
	} else if (h1 <= 3) {
		red = 0;
		green = c;
		blue = x;
	} else if (h1 <= 4) {
		red = 0;
		green = x;
		blue = c;
	} else if (h1 <= 5) {
		red = x;
		green = 0;
		blue = c;
	} else if (h1 <= 6) {
		red = c;
		green = 0;
		blue = x;
	}
	rgba_t color = {
		.red = (int) (255.0 * red),
		.green = (int) (255.0 * green),
		.blue = (int) (255.0 * blue),
		.transparency = 0,
	};
	return color;
}

pub typedef {
	int color_width;
	size_t size;
	rgba_t colors[20];
} colormap_t;

pub rgba_t mapcolor(colormap_t *c, int val) {
	int w = c->color_width;
	// The first color is special, reserved for low values (think background).
	if (val <= w) {
		return mix(c->colors[0], c->colors[1], (double)val/(double)w);
	}
	// For larger values we exlude the first color and map to the rest.
	return mapcolor_(c->colors+1, c->size-1, w, val-w);
}

rgba_t mapcolor_(rgba_t *colors, size_t ncolors, int color_width, int val) {
	int index1 = (val / color_width) % ncolors;
	int index2 = (index1 + 1) % ncolors;
	double rate = (double) (val % color_width) / (double) color_width;
	return mix(colors[index1], colors[index2], rate);
}
