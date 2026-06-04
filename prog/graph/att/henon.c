#import image
#import rnd

const int N = 20000;
const double PI = 3.141592653589793238462643383279502884197169399375105820974944;
const double TWOPI = 2 * PI;

typedef { double x, y; } point_t;

typedef {
	double m;
} params_t;

pub void *newparams() {
	params_t *p = calloc!(1, sizeof(params_t));
	p->m = 2;
	return p;
}

pub void mutateparams(void *state) {
	params_t *p = state;
	p->m += 0.1;
	if (p->m > 12) p->m = 2;
}

pub void draw(image.image_t *img, void *state) {
	params_t *p = state;
	double m = p->m;
	if (m < 2 || m > 12) {
		panic("m out of range, must be between 2 and 12");
	}
	double a[25] = {};
	double b[25] = {};
	for (int i=0;i<(int)m;i++) {
		a[i] = cos(TWOPI * i / (double)m);
		b[i] = sin(TWOPI * i / (double)m);
	}

	point_t q = {1, 1};
	
	for (int n=0;n<N;n++) {
		q = next(a, b, (int)m, q);
		if (n < 100) {
			continue;
		}
		// assume x, y are in [-2, 2]
		int ix = (int) ((q.x + 2)/4 * img->width);
		int iy = (int) ((q.y + 2)/4 * img->height);
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
