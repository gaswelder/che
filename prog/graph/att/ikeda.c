#import complex
#import image

// Draws an ikeda picture.
pub void draw(image.image_t *img, double a, b, k, p) {
	complex.t z = {0};
	image.rgba_t color = image.white();
	int n = 100000;
	for (int i = 0; i < n; i++) {
		double x = 240 + 180*z.re;
		double y = 280 + 180*z.im;
		image.set(img, (int) x, (int) y, color);
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
