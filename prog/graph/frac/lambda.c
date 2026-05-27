#import image
#import math/complex

double max = 100;
int it=35;

pub void draw(image.image_t *img) {
  int mx = img->width / 2;
  int my = img->height / 2;
  for (int x = -mx; x <= mx; x++) {
    for (int y = -my; y <= my; y++) {
		complex.t z = {x * 0.01, y * 0.01};
    	complex.t l = {0.85, 0.6};
    	int k = 0;
		while (k < it && complex.abs(z) < max) {
			complex.t tmp = complex.mul(l, z);
			tmp = complex.mul(tmp, complex.diff(complex.make(1, 0), z));
			z = tmp;
			k++;
		}
    	if (k<it) {
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
