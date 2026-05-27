#import image
#import math/complex

// Draws an ikeda picture.
pub void draw(image.image_t *img, double a, b, k, p) {
	complex.t z = {0};
	image.rgba_t color = image.white();
	int n = 100000;
	double w = img->width;
	double h = img->height;
	for (int i = 0; i < n; i++) {
		double x = z.re;
		double y = z.im;
		int ix = (int) ((x + 1) / 3.0 * w);
		int iy = (int) ((y + 2) / 4.0 * h);
		if (ix >= 0 && ix < img->width && iy >= 0 && iy < img->height) {
			image.set(img, ix, iy, color);
		}
		z = next(z, a, b, k, p);
	}
}

// next(z) = a + b * z * exp(i * (k - p/(1 + z*z)));
complex.t next(complex.t z, double a, b, k, p) {
	double bar = k - p / (1 + complex.abs2(z));
	complex.t foo = {0, bar};
    complex.t r = z;
	r = complex.mul(r, compexp(foo));
	r = complex.scale(r, b);
	r.re += a;
	return r;
}

complex.t compexp(complex.t x) {
	// exp(a + ib)
	// = exp(a) * exp(ib)
	// = exp(a) * (cos(b) + i sin(b))
	double expa = exp(x.re);
	complex.t r = {
		.re = cos(x.im) * expa,
		.im = sin(x.im) * expa
	};
	return r;
}
