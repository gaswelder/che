#import image
#import rnd

const int N = 20000;
const double PI = 3.141592653589793238462643383279502884197169399375105820974944;
const double TWOPI = 2 * PI;

typedef { double x, y; } point_t;

pub void draw(image.image_t *img, double m) {
	if (m < 2 || m > 12) {
		panic("m out of range, must be between 2 and 12");
	}
	double a[25] = {};
	double b[25] = {};
	for (int i=0;i<(int)m;i++) {
		a[i] = cos(TWOPI * i / (double)m);
		b[i] = sin(TWOPI * i / (double)m);
	}

	point_t p = {1, 1};
	
	for (int n=0;n<N;n++) {
		p = next(a, b, m, p);
		if (n < 100) {
			continue;
		}
		// assume x, y are in [-2, 2]
		int ix = (int) ((p.x + 2)/4 * img->width);
		int iy = (int) ((p.y + 2)/4 * img->height);
		image.set(img, ix, iy, image.white());
	}
}

point_t next(double *a, *b, int m, point_t p) {
	int l = rnd.intn(m);
	point_t r = {};
	if (rnd.intn(2) == 0) {
		r.x = p.x / 2.0 + a[l];
		r.y = p.y / 2.0 + b[l];
	} else {
		r.x = p.x * a[l] + p.y * b[l] + p.x * p.x * b[l];
		r.y = p.y * a[l] - p.x * b[l] + p.x * p.x * a[l];
		r.x /= 6;
		r.y /= 6;
	}
	return r;
}
