#import att/dejong.c
#import att/ikeda.c
#import att/pickover.c
#import frac/thorn.c
#import image
#import opt
#import render.c
#import rnd

pub int run(int argc, char *argv[]) {
	char *size = "400x400";
	opt.nargs(1, "<algorithm = thorn / pickover / dejong / ikeda>");
	opt.str("s", "image size", &size);
	opt.parse(argc, argv);

	int width;
	int height;
	if (sscanf(size, "%dx%d", &width, &height) != 2) {
		fprintf(stderr, "failed to parse the size\n");
		return 1;
	}

	const int FRAMES = 1000;

	image.image_t *img = image.new(width, height);

	switch str (argv[1]) {
		case "thorn": {
			double cr = (rnd.intn(10000)) / 500.0 - 10; // -10 -> 10;
			double ci = (rnd.intn(10000)) / 500.0 - 10;
			for (int i = 0; i < FRAMES; i++) {
				ci += 0.1;
				cr += 0.1;
				thorn.draw(img, cr, ci);
				render.push(img);
			}
		}
		case "pickover": {
			for (int i = 0; i < FRAMES; i++) {
				image.apply(img, fade);
				pickover.draw(img, i, FRAMES);
				render.push(img);
			}
		}
		case "dejong": {
			double a = -2.70;
			double b = -0.90;
			double c = -0.86;
			double d = -2.20;
			for (int i = 0; i < FRAMES; i++) {
				dejong.draw(img, a, b, c, d);
				render.push(img);
				a += rnd.u() / 1000;
				b += rnd.u() / 1000;
				c += rnd.u() / 1000;
				d += rnd.u() / 1000;
				image.apply(img, fade);
			}
		}
		case "ikeda": {
			double a = 0.85;
			double b = 0.9;
			double k = 0.4;
			double p = 7.7;
			for (int i = 0; i < FRAMES; i++) {
				image.clear(img);
				ikeda.draw(img, a, b, k, p);
				render.push(img);
				a -= 0.001;
				// b += 0.0001;
				// k += 0.001;
				p += 0.01;
			}
		}
		default: {
			fprintf(stderr, "unknown algorithm\n");
			return 1;
		}
	}

	
	render.end();
	image.free(img);
	return 0;
}

void fade(image.rgba_t *c) {
	c->red /= 2;
	c->green /= 2;
	c->blue /= 2;
}
