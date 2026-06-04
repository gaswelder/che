#import image
#import rnd

// Clifford Pickover's attractor.

typedef {
	double a, b, c, d;
} params_t;

pub void *newparams() {
	params_t *p = calloc!(1, sizeof(params_t));
	// example:
	// a = -1.4, b = 1.6, c = 1.0, d = 0.7
	p->a = rnd.u() * 4 - 2;
	p->b = rnd.u() * 4 - 2;
	p->c = rnd.u() * 4 - 2;
	p->d = rnd.u() * 4 - 2;
	return p;
}

pub void mutateparams(void *state) {
	double step = 4.0 / 1000; // assuming 1000 frames
	params_t *p = state;
	p->a = circle(p->a + step, -2, 2);
	p->b = circle(p->b + step, -2, 2);
	p->c = circle(p->c + step, -2, 2);
	p->d = circle(p->d + step, -2, 2);
}

double circle(double x, min, max) {
	if (x > max) {
		return min + x - max;
	}
	return x;
}

pub void draw(image.image_t *img, void *state) {
	params_t *p = state;
	double a = p->a;
	double b = p->b;
	double c = p->c;
	double d = p->d;
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
