#import image
#import rnd

typedef {
	int size;
} params_t;

pub void *newparams() {
	params_t *p = calloc!(1, sizeof(params_t));
	p->size = 0;
	return p;
}

pub void mutateparams(void *state) {
	(void) state;
}

pub void draw(image.image_t *img, void *state) {
	params_t *p = state;
	// Initialize the size on the first call from image size.
	// The size must be 2^n+1.
	if (p->size == 0) {
		int sz = 2;
		while (sz*2 + 1 <= img->width) {
			sz *= 2;
		}
		p->size = sz;
	}
	int size = p->size;

	// Initialize the corners.
	image.set(img, 0, 0, image.gray(rnd.intn(100)));
	image.set(img, 0, size-1, image.gray(rnd.intn(100)));
	image.set(img, size-1, 0, image.gray(rnd.intn(100)));
	image.set(img, size-1, size-1, image.gray(rnd.intn(100)));

	// Smoothness, 0..1
	double h = 0.4;

	double fac = pow(2, -h);
	int randscale = 200;

	int tilesize = size;
	while (tilesize >= 3) {
		int h = tilesize / 2;

		// Diamonds.
		for (int x = 0; x < size-1; x += tilesize-1) {
			for (int y = 0; y < size-1; y += tilesize-1) {
				diamond(img, x+h, y+h, h, randscale);
			}
		}

		// Squares.
		for (int x = 0; x < size-1; x += tilesize-1) {
			for (int y = 0; y < size-1; y += tilesize-1) {
				int cx = x + h;
				int cy = y + h;
				square(img, cx-h, cy, h, randscale);
				square(img, cx+h, cy, h, randscale);
				square(img, cx, cy-h, h, randscale);
				square(img, cx, cy+h, h, randscale);
			}
		}

		tilesize = tilesize / 2 + 1;
		randscale = (int) ((double) randscale * fac);
	}
}

// Calculates center (x, y) of a square of half-width h using its corners.
void diamond(image.image_t *img, int x, y, h, randscale) {
	int r = 0;
	if (randscale > 0) {
		r += rnd.intn(randscale);
	}
	r += image.get(img, x - h, y - h).red;
	r += image.get(img, x + h, y - h).red;
	r += image.get(img, x - h, y + h).red;
	r += image.get(img, x + h, y + h).red;
	r /= 4;
	image.set(img, x, y, image.gray(wrap(r)));
}

// Calculates a square's edge at (x, y) using centers of adjacent squares.
void square(image.image_t *img, int x, y, h, randscale) {
	int r = 0;
	if (randscale > 0) {
		r += rnd.intn(randscale);
	}
	int n = 0;
	if (x > 0) {
		r += image.get(img, x - h, y).red;
		n++;
	}
	if (y > 0) {
		r += image.get(img, x, y - h).red;
		n++;
	}
	if (x < img->width-1) {
		r += image.get(img, x + h, y).red;
		n++;
	}
	if (y < img->height-1) {
		r += image.get(img, x, y + h).red;
		n++;
	}
	r /= n;
	image.set(img, x, y, image.gray(wrap(r)));
}

// This is an ad-hoc invention, wraps values that overflow the max.
int wrap(int x) {
	if (x <= 255) return x;
	int diff = x - 255;
	if (diff > 255) panic("!");
	return x - diff;
}
