#import image
#import rnd

const int iter = 10000;

// const double   a  = 0.00;           // slope
// const double   b  = 0.70;           // leaf size coefficient
// const double   c  = 0.70;           // dispersion
// const double   d  = 0.00;           // skew
typedef {
	double a, b, c, d;
} params_t;

pub void *newparams() {
	params_t *p = calloc!(1, sizeof(params_t));
	p->a = 0.00;
	p->b = 0.70;
	p->c = 0.70;
	p->d = 0.00;
	return p;
}

pub void mutateparams(void *state) {
	params_t *p = state;
	p->c += 0.001;
	p->d += 0.001;
}

pub void draw(image.image_t *img, void *state) {
	params_t *p = state;
	double a = p->a;
	double b = p->b;
	double c = p->c;
	double d = p->d;
	int rad = img->width / 2;
	float x = 0.0;
	float y = 0.0;

	for (int k = 1; k <= iter; k++) {
		float t = x;
		if (rnd.intn(2) == 0) {
			x = a * x - b * y;
			y = b * t + a * y;
		} else {
			x = c * x - d * y + 1 - c;
			y = d * t + c * y - d;
		}
		int ix = img->width / 2 + (int)(rad * x);
		int iy = img->height / 2 + (int)(rad * y);

		if (ix >= 0 && ix < img->width && iy >= 0 && iy < img->height) {
			image.set(img, ix, iy, image.white());
		}
	}
}
