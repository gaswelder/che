#import image
 
typedef {
	int it;
} params_t;

pub void *newparams() {
	params_t *p = calloc!(1, sizeof(params_t));
	p->it = 1000;
	return p;
}

pub void mutateparams(void *state) {
	params_t *p = state;
	p->it += 100;
}

pub void draw(image.image_t *img, void *state) {
	params_t *p = state;
	int it = p->it;
	double x = 0;
	double y = 0;
	int n = 0;
	int hw = img->width / 2;
	int hh = img->height / 2;
	for (int i = 0; i < it; i++) {
		double t = x;
		x = y - sin(x);
		y = 3.14 - t;
		n++;
		image.set(img, hw + (int)(x*2), hh + (int)(y*2), image.white());
	}
}
