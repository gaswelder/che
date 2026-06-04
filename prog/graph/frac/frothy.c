#import image
#import math/complex

const int ITERATIONS = 50;

// double max = 16;
// c.im = 1.02871;
typedef {
	double cim;
	double max;
} params_t;

pub void *newparams() {
	params_t *p = calloc!(1, sizeof(params_t));
	p->cim = 1;
	p->max = 0;
	return p;
}

pub void mutateparams(void *state) {
	params_t *p = state;
	p->cim += 0.001;
	p->max += 1;
}

pub void draw(image.image_t *img, void *state) {
	params_t *p = state;
	double max = p->max;
	double cim = p->cim;
	int mx = img->width / 2;
	int my = img->height / 2;
	for (int y = -my; y < mx; y++) {
		for (int x = -mx; x < mx; x++) {
			complex.t z = {.re = 0.01*x, .im = 0.01*y};
			complex.t c = {.re = 1, .im = cim};
			int n = 0;
			while (complex.abs2(z) < max && n < ITERATIONS) {
				complex.t z1 = z;
				z = complex.mul(z1, z1);
				complex.t mm = {
					.re = c.re*(z1.re + z1.im),
					.im = c.im*(z1.re - z1.im)
				};
				z = complex.diff(z, mm);
				n++;
			}
			double val = complex.abs2(z);
			double norm = val / 472;  // Assume it's 0..472.
			image.set(img, mx + x, my + y, image.gray((int) (norm * 255)));
		}
	}
}
