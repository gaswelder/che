pub typedef { int w, h; } dim_t;
pub typedef { int red, green, blue; } rgb_t;

pub typedef {
	int width;
	int height;
	rgb_t *data;
} image_t;

pub rgb_t *getpixel(image_t *img, int x, y) {
	return &img->data[y * img->width + x];
}

pub void set(image_t *img, int x, y, rgb_t c) {
	*getpixel(img, x, y) = c;
}

pub rgb_t get(image_t *img, int x, y) {
	return *getpixel(img, x, y);
}

pub image_t *new(int width, height) {
	image_t *img = calloc(1, sizeof(image_t));
	if (!img) panic("calloc failed");
	img->width = width;
	img->height = height;
	img->data = calloc(width * height, sizeof(rgb_t));
	if (!img->data) panic("calloc failed");
	return img;
}

pub void free(image_t *img) {
	OS.free(img->data);
	OS.free(img);
}
