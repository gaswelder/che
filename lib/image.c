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
	image_t *img = calloc(1, sizeof(image_t));
	if (!img) panic("calloc failed");
	img->width = width;
	img->height = height;
	img->data = calloc(width * height, sizeof(rgba_t));
	if (!img->data) panic("calloc failed");
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
    rgba_t newcolor = get(img, x, y);
    newcolor.red = opacity * color.red + (opacity - 1) * newcolor.red;
    newcolor.green = opacity * color.green + (opacity - 1) * newcolor.green;
    newcolor.blue = opacity * color.blue + (opacity - 1) * newcolor.blue;
    set(img, x, y, newcolor);
}

// Applies f to every pixel of the image.
pub void apply(image_t *img, pixelfunc_t *f) {
	for (int y = 0; y < img->height; y++) {
		for (int x = 0; x < img->width; x++) {
			rgba_t color = get(img, x, y);
			f(&color);
			set(img, x, y, color);
		}
	}
}

pub rgba_t white() {
	rgba_t c = {255, 255, 255, 0};
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
