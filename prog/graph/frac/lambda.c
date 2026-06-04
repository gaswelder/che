#import image
#import math/complex

typedef {
	double max;
	int it;
	double l_re;
	double l_im;
} params_t;

pub void *newparams() {
	params_t *p = calloc!(1, sizeof(params_t));
	p->max = 100;
	p->it = 35;
	p->l_re = 0.85;
	p->l_im = 0.6;
	return p;
}

pub void mutateparams(void *state) {
	(void) state;
}

pub void draw(image.image_t *img, void *state) {
	params_t *p = state;
	double max = p->max;
	int it = p->it;
	double l_re = p->l_re;
	double l_im = p->l_im;

	int mx = img->width / 2;
	int my = img->height / 2;
	for (int x = -mx; x <= mx; x++) {
		for (int y = -my; y <= my; y++) {
			complex.t z = {x * 0.01, y * 0.01};
			complex.t l = {l_re, l_im};
			int k = 0;
			while (k < it && complex.abs(z) < max) {
				complex.t tmp = complex.mul(l, z);
				tmp = complex.mul(tmp, complex.diff(complex.make(1, 0), z));
				z = tmp;
				k++;
			}
			if (k < it) {
				int ix = mx + x;
				int iy = my + y;
				if (ix >= 0 && ix < img->width && iy >= 0 && iy < img->height) {
					// k is in [2..34]
					int val = (k % 16);
					image.set(img, mx+x, my+y, image.gray(val * 16));
				}
			}
		}
	}
}
