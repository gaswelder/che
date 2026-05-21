#import image

// Clifford Pickover's attractor.

pub void draw(image.image_t *img, double a, b, c, d) {
	double WIDTH = img->width;
	double HEIGHT = img->height;

	const double minX = -4.0;
	const double maxX = 4.0;
	const double minY = minX * HEIGHT / WIDTH;
	const double maxY = maxX * HEIGHT / WIDTH;

	image.rgba_t color = image.white();
	point_t q = {};
	for (int j = 0; j < 10000; j++) {
		q = next(q, a, b, c, d);
		int xi = (int) ((q.x - minX) * WIDTH / (maxX - minX));
		int yi = (int) ((q.y - minY) * HEIGHT / (maxY - minY));
		image.set(img, xi, yi, color);
	}
}

typedef {
	double x, y;
} point_t;

point_t next(point_t p, double a, b, c, d) {
	point_t r = {0.0, 0.0};
	r.x = sin(a * p.y) + c * cos(a * p.x);
	r.y = sin(b * p.x) + d * cos(b * p.y);
	return r;
}
