#import image
#import opt
#import render.c

pub int run(int argc, char *argv[]) {
    char *size = "200x200";
	opt.nargs(0, "");
	opt.str("s", "image size", &size);
	opt.parse(argc, argv);

	int width;
	int height;
	if (sscanf(size, "%dx%d", &width, &height) != 2) {
		fprintf(stderr, "failed to parse the size\n");
		return 1;
	}

    const int FRAMES = 10000;

	image.image_t *img = image.new(width, height);
    for (int i = 0; i < FRAMES; i++) {
		image.apply(img, fade);
		draw(img, i, FRAMES);
		render.push(img);
    }
	render.end();
    return 0;
}

void draw(image.image_t *img, int i, FRAMES) {
	int WIDTH = img->width;
	int HEIGHT = img->height;

	const int iters = 10000;
    const double minX = -4.0;
    const double minY = minX * HEIGHT / WIDTH;
    const double maxX = 4.0;
    const double maxY = maxX * HEIGHT / WIDTH;
    const double minA = acos( 1.6 / 2.0 );
    const double maxA = acos( 1.3 / 2.0 );
    const double minB = acos( -0.6 / 2.0 );
    const double maxB = acos( 1.7 / 2.0 );
    const double minC = acos( -1.2 / 2.0 );
    const double maxC = acos( 0.5 / 2.0 );
    const double minD = acos( 1.6 / 2.0 );
    const double maxD = acos( 1.4 / 2.0 );

	double p = (double) i / FRAMES;
	double a = cos( minA + p * (maxA - minA) ) * 2.0;
	double b = cos( minB + p * (maxB - minB) ) * 2.0;
	double c = cos( minC + p * (maxC - minC) ) * 2.0;
	double d = cos( minD + p * (maxD - minD) ) * 2.0;
	image.rgba_t color = create_hue(p);
	double x = 0.0;
	double y = 0.0;
	for (int j = 0; j < iters; j++) {
		// Calculate next (x, y) from current (x, y).
		// xn+1 = sin(a yn) + c cos(a xn)
		// yn+1 = sin(b xn) + d cos(b yn)
		double xn = sin(a * y) + c * cos(a * x);
		double yn = sin(b * x) + d * cos(b * y);
		x = xn;
		y = yn;

		double wtfx = (x - minX) * WIDTH / (maxX - minX);
		double wtfy = (y - minY) * HEIGHT / (maxY - minY);
		int xi = (int) wtfx;
		int yi = (int) wtfy;
		if ( xi >= 0 && xi < WIDTH && yi >= 0 && yi < HEIGHT ) {
			image.blend(img, xi, yi, color, 0.9);
		}
	}
}

void fade(image.rgba_t *c) {
	c->red /= 2;
	c->green /= 2;
	c->blue /= 2;
}

image.rgba_t create_hue(double h) {
    h *= 6.0;
	int inth = (int)h;
    double hf = h - (double) inth;
    image.rgba_t r = {};
    switch (inth % 6) {
        case 0: { r.red = 255; r.green = 255 * hf; r.blue = 0; }
        case 1: { r.red = 255 * (1.0 - hf); r.green = 255; r.blue = 0; }
        case 2: { r.red = 0; r.green = 255; r.blue = 255 * hf; }
        case 3: { r.red = 0; r.green = 255 * (1.0 - hf); r.blue = 255; }
        case 4: { r.red = 255 * hf; r.green = 0; r.blue = 255; }
        case 5: { r.red = 255; r.green = 0; r.blue = 255 * (1.0 - hf); }
    }
    return r;
}
