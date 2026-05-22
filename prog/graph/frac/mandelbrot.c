#import image
#import complex

// The Mandelbrot set is based on the function
//
//      f(z, c) = z^2 + c.
//
// The variable c is reinterpreted as a pixel coordinate: x=Re(c), y=Im(c).
// For each sample point c we are looking at how quickly the sequence
//
//      |f(0, c)|, |f(0, f(0, c))|, ...
//
// goes to infinity. Pixels may be colored then according to how soon the
// sequence crosses a chosen threshold. The threshold should be higher than 2
// (which is max(abs(f(any, any)))).

// * If c is held constant and the initial value of z is varied instead,
// we get a Julia set instead.

// log(2)
const double logtwo = 0.693147180559945;

pub typedef { double xmin, xmax, ymin, ymax; } area_t;

pub void draw(image.image_t *img, image.colormap_t *cm, area_t area, int it) {
	int width = img->width;
	int height = img->height;
	double xres = (area.xmax - area.xmin) / width;
	double yres = (area.ymax - area.ymin) / height;
	for (int j = 0; j < height; j++) {
		for (int i = 0; i < width; i++) {
			complex.t c = {
				.re = area.xmin + i * xres,
				.im = area.ymin + j * yres
			};
			double v = get_val(c, it);
			*image.getpixel(img, i, j) = image.mapcolor(cm, v);
		}
	}
}

double get_val(complex.t c, int it) {
	complex.t z = { 0, 0 };

	int count = 0;
	while (complex.abs2(z) < 4 && count < it) {
		/* Z = Z^2 + C */
		z = complex.sum( complex.mul(z, z), c );
		count++;
	}
	if (complex.abs2(z) < 4) {
		return 0;
	}
	return smooth(count, complex.abs(z));
}

double smooth(int count, double amp) {
	double off = (double) count + 1;
	return off - log(log(amp)) / logtwo;
}
