#import image

// Sample:
// double a = -2.70;
// double b = -0.90;
// double c = -0.86;
// double d = -2.20;

pub void draw(image.image_t *img, double a, b, c, d) {
	image.rgba_t color = image.white();
	double x = 0;
	double y = 0;
	double x1 = 0;
	double y1 = 0;
	double w = img->width;
	double h = img->height;
	for (int i = 0; i < 1000000; i++) {
		x1 = sin(a * y) - cos(b * x);
		y1 = sin(c * x) - cos(d * y);
		x = x1;
		y = y1;
		int ix = (int)( (x + 2.0) / 4.0 * w );
		int iy = (int)( (y + 2.0) / 4.0 * h );
		image.set(img, ix, iy, color);
	}
}
