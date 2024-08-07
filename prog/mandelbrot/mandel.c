#import complex

/* Calculated log (2) beforehand */
const double logtwo = 0.693147180559945;

pub double *gen(int width, height, double xmin, xmax, ymin, ymax, int it) {
	double xres = (xmax - xmin) / width;
	double yres = (ymax - ymin) / height;

	double *data = calloc (width * height, sizeof (double));
	for (int j = 0; j < height; j++) {
		for (int i = 0; i < width; i++) {
			complex.t c = {
				.re = xmin + i * xres,
				.im = ymin + j * yres
			};
			data[i + j * width] = get_val (c, it);
		}
	}
	return data;
}

// The Mandelbrot set is based on the function
//
//      f(z, c) = z^2 + c.
//
// The variable c is reinterpreted as a pixel coordinate: x=Re(c), y=Im(c).
// For each sample point c we're looking how quickly the sequence
//
//      |f(0, c)|, |f(0, f(0, c))|, ...
//
// goes to infinity. Pixels may be colored then according to how soon the
// sequence crosses a chosen threshold. The threshold should be higher than 2
// (which is max(abs(f(any, any)))).

// * If c is held constant and the initial value of z is varied instead,
// we get a Julia set instead.

double get_val (complex.t c, int it) {
	complex.t z = { 0, 0 };

	int count = 0;
	while ((z.re * z.re + z.im * z.im < 2 * 2) && count < it) {
		/* Z = Z^2 + C */
		z = complex.sum( complex.mul(z, z), c );
		count++;
	}
	if (z.re * z.re + z.im * z.im < 2 * 2)
		return 0;

	/* Smooth coloration */
	double out = (double) (count + 1 - log (log (sqrt (z.re * z.re + z.im * z.im))) / logtwo);
	return out;
}
