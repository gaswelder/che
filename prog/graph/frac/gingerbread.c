#import image

typedef {
	int iterations;
} params_t;

pub void *newparams() {
	params_t *p = calloc!(1, sizeof(params_t));
	p->iterations = 1;
	return p;
}

pub void mutateparams(void *state) {
	params_t *p = state;
	if (p->iterations < 10000) {
		p->iterations += 100;
	}
}

pub void draw(image.image_t *img, void *state) {
	params_t *p = state;
	int iterations = p->iterations;
	float x = -0.1;
	float y = 0;
	int n = 0;

	for (int i = 0; i < iterations; i++) {
		float t = x;
		x = 1 - y + fabs(x);
		y = t;
		n++;
		int ix = img->width / 2 + ((int) (20 * x));
		int iy = img->height / 2 + ((int) (20 * y));
		if (ix >= 0 && ix < img->width && iy >= 0 && iy < img->height) {
			image.set(img, ix, iy, image.white());
		}
	}
}
