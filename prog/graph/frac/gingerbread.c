#import image

pub void draw(image.image_t *img, int iterations) {
	float x=-0.1;
	float y=0;
	int n=0;

	for (int i = 0; i < iterations; i++) {
		float t=x;
		x=1-y+fabs(x);
		y=t;
		n++;
		int ix = img->width / 2 + ((int) (20 * x));
		int iy = img->height / 2 + ((int) (20 * y));
		if (ix >= 0 && ix < img->width && iy >= 0 && iy < img->height) {
			image.set(img, ix, iy, image.white());
		}
	}
}
