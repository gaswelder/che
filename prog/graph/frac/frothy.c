#import image
#import math/complex

const int ITERATIONS = 50;

// double max = 16;
// c.im = 1.02871;

pub void draw(image.image_t *img, double max, double cim) {
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
			double norm = val / 472; // Assume it's 0..472.
			image.set(img, mx + x, my + y, image.gray((int) (norm * 255)));
		}
	}
}
