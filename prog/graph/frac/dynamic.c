#import image

typedef {
	float a, b, dt;
} params_t;

pub void *newparams() {
	params_t *p = calloc!(1, sizeof(params_t));
	p->a = 1;
	p->b = 3;
	p->dt = 0.1;
	return p;
}

pub void mutateparams(void *state) {
	params_t *p = state;
	p->b += 0.01;
}

pub void draw(image.image_t *img, void *state) {
	params_t *p = state;
	float a = p->a;
	float b = p->b;
	float dt = p->dt;

	for (int i = 0; i <= 43; i++) {
		for (int j = 0; j <= 37; j++) {
			float x = i;
			float y = j;
			for (int k = 1; k <= 100; k++) {
				y = y + sin(x + a * sin(b * x)) * dt;
				x = x - sin(y + a * sin(b * y)) * dt;
				int val = 200 + ((i+j)%10);
				int ix = (int)(x*15);
				int iy = (int)(y*15);
				if (ix >= 0 && ix < img->width && iy >= 0 && iy < img->height) {
					image.set(img, ix, iy, image.gray(val));
				}
			}
		}
	}
}
