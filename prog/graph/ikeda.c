#import complex
#import image
#import render.c

pub int run(int argc, char **argv) {
	(void) argc;
	(void) argv;
	int width = 800;
	int height = 600;

	image.image_t *img = image.new(width, height);
	for (int i = 0; i < 100; i++) {
		image.clear(img);
		ikeda(img);
		render.push(img);
		a -= 0.001;
		// b += 0.0001;
		// k += 0.001;
		p += 0.01;
	}
	image.free(img);
	return 0;
}

void ikeda(image.image_t *img) {
	complex.t z = {0};
	image.rgba_t color = image.white();
	int n = 100000;
	for (int i = 0; i < n; i++) {
		double x = 240 + 180*z.re;
		double y = 280 + 180*z.im;
		image.set(img, (int) x, (int) y, color);
		z = next(z);
	}
}

double a = 0.85;
double b = 0.9;
double k = 0.4;
double p = 7.7;

// next(z) = a + b * z * exp(i * (k - p/(1 + z*z)));
complex.t next(complex.t z) {
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
