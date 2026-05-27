#import image
#import math/complex 

// "Frothy"

const int iter = 50;

pub void draw(image.image_t *img, int max, double cim) {
	// int max = 16;

	complex.t z = {};
	complex.t z1 = {};
	complex.t c = {};

	int x;
	int y;
	int n;

	int mx = img->width / 2;
	int my = img->height / 2;

	for (y=-my; y<mx; y++) {
		for (x=-mx; x< mx; x++) {
			
			z.re = x*0.01;
			z.im = y*0.01;
			c.re = 1;
			// c.im = 1.02871;
			c.im = cim;

			n=0;
			while (true) {
				bool ok =  (((z.re*z.re) + (z.im*z.im)) < max) && (n < iter);
				if (!ok) {
					break;
				}
				z1 = z;
				z.re = (z1.re*z1.re) - (z1.im*z1.im) - c.re*(z1.re + z1.im);
				z.im = 2*z1.re*z1.im - c.im*(z1.re - z1.im);
				n++;
			}
	  		if (n=iter) {
				double val = complex.abs2(z); // 0..472
				image.set(img, mx + x, my + y, image.gray((int) (val / 472 * 256)));
			}
		}
	}
}
