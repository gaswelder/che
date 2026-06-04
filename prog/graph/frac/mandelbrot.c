#import image
#import math/complex

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
// we get a Julia set.

// log(2)
const double logtwo = 0.693147180559945;

typedef {
	image.colormap_t *cm;
	double zoomx, zoomy;
	double hw, hh;
	double zoom_rate;
	int iterations;
} params_t;

pub void *newparams() {
	params_t *p = calloc!(1, sizeof(params_t));

	image.rgba_t colors[] = {
		{ 0, 0, 0, 0},
		{ 0, 0, 255, 0},
		{ 0, 128, 255, 0},
		{ 0, 255, 128, 0},
		{ 128, 128, 0, 0},
		{ 255, 128, 0, 0},
		{ 255, 255, 128, 0},
		{ 255, 255, 255, 0}
	};
	image.colormap_t *cm = calloc!(1, sizeof(image.colormap_t));
	cm->size = nelem(colors);
	cm->color_width = 50;
	for (size_t i = 0; i < nelem(colors); i++) {
		cm->colors[i] = colors[i];
	}
	p->cm = cm;
	p->zoomx = -1.268794803623;
	p->zoomy = 0.353676833206;
	p->hw = (1.5 - (-2.5)) / 2;
	p->hh = (1.5 - (-1.5)) / 2;
	p->zoom_rate = 0.1;
	p->iterations = 64;
	return p;
}

pub void mutateparams(void *state) {
	params_t *p = state;
	p->hw /= (1 + p->zoom_rate);
	p->hh /= (1 + p->zoom_rate);
}

pub typedef { double xmin, xmax, ymin, ymax; } area_t;

pub void draw(image.image_t *img, void *state) {
	params_t *p = state;
	area_t a = {
		.xmin = p->zoomx - p->hw,
		.xmax = p->zoomx + p->hw,
		.ymin = p->zoomy - p->hh,
		.ymax = p->zoomy + p->hh,
	};

	int width = img->width;
	int height = img->height;
	double xres = (a.xmax - a.xmin) / width;
	double yres = (a.ymax - a.ymin) / height;
	for (int j = 0; j < height; j++) {
		for (int i = 0; i < width; i++) {
			complex.t c = {
				.re = a.xmin + i * xres,
				.im = a.ymin + j * yres
			};
			double v = get_val(c, p->iterations);
			*image.getpixel(img, i, j) = image.mapcolor(p->cm, v);
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
	return smooth((double) count, complex.abs(z));
}

double smooth(double count, double amp) {
	double off = count + 1;
	return off - log(log(amp)) / logtwo;
}
