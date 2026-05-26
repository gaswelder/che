#import att/dejong.c
#import att/henon.c
#import att/ikeda.c
#import att/pickover.c
#import frac/dendrite.c
#import frac/diamondsquare.c
#import frac/frothy.c
#import frac/gingerbread.c
#import frac/lambda.c
#import frac/mandelbrot.c
#import frac/martin.c
#import frac/thorn.c
#import image
#import opt
#import render.c
#import rnd

int main(int argc, char *argv[]) {
	char *size = "400x400";
	opt.nargs(1, "<algorithm = thorn / pickover / dejong / ikeda / mandelbrot / ...>");
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
			// a = -1.4, b = 1.6, c = 1.0, d = 0.7
			double a = rnd.u() * 4 - 2;
			double b = rnd.u() * 4 - 2;
			double c = rnd.u() * 4 - 2;
			double d = rnd.u() * 4 - 2;
			double step = 4.0 / FRAMES;

			for (int i = 0; i < FRAMES; i++) {
				image.apply(img, fade);
				pickover.draw(img, a, b, c, d);
				render.push(img);

				a = circle(a + step, -2, 2);
				b = circle(b + step, -2, 2);
				c = circle(c + step, -2, 2);
				d = circle(d + step, -2, 2);
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
		case "mandelbrot": {
			image.rgba_t colors[] = {
				{ 0, 0, 0, 0},
				{ 0, 0, 255, 0},
				{ 0, 128, 255, 0},
				{ 0, 255, 128, 0},
				{ 128, 128, 0, 0},
				{ 255, 128, 0, 0},
				{ 255, 255, 128, 0},
				{ 255, 255, 255, 0}
			};
			image.colormap_t *cm = calloc!(1, sizeof(image.colormap_t));
			cm->size = nelem(colors);
			cm->color_width = 50;
			for (size_t i = 0; i < nelem(colors); i++) {
				cm->colors[i] = colors[i];
			}
			image.colormap_t *GLOBAL_CM = cm;

			double config_xmin = -2.5;
			double config_xmax = 1.5;
			double config_ymin = -1.5;
			double config_ymax = 1.5;
			double hw = (config_xmax - config_xmin) / 2;
			double hh = (config_ymax - config_ymin) / 2;
			int iterations = 64; // 512
			double zoom_rate = 0.1;
			double zoomx = -1.268794803623;
			double zoomy = 0.353676833206;

			for (int i = 0; i < FRAMES; i++) {
				fprintf(stderr, "%d / %d\n", i, FRAMES);

				// The first frame spans [center-hwidth, center+hwidth]
				// The next frame spans [center-hwidth/(1+zoom_rate), center+hwidth/(1+zoom_rate)]
				mandelbrot.area_t a = {};
				a.xmin = zoomx - hw;
				a.xmax = zoomx + hw;
				a.ymin = zoomy - hh;
				a.ymax = zoomy + hh;
				hw /= (1 + zoom_rate);
				hh /= (1 + zoom_rate);

				mandelbrot.draw(img, GLOBAL_CM, a, iterations);
				render.push(img);				
			}
		}
		case "frothy": {
			double cim = 1;
			for (int i = 0; i < FRAMES; i++) {
				cim += 0.001;
				image.clear(img);
				frothy.draw(img, i, cim);
				render.push(img);
			}
		}
		case "henon": {
			// Normally an integer [2..12], but we're animating.
			double m = 2;
			for (int i = 0; i < FRAMES; i++) {
				image.clear(img);
				henon.draw(img, m);
				render.push(img);
				m += 0.1;
				if (m > 12) m = 2;
			}
		}
		case "gingerbread": {
			int iterations = 1;
			for (int i = 0; i < FRAMES; i++) {
				image.clear(img);
				gingerbread.draw(img, iterations);
				render.push(img);
				if (iterations < 10000) {
					iterations += 100;
				}
			}
		}
		case "dendrite": {
			double a = 0.00; // slope
			double b = 0.70; // leaf size coefficient
			double c = 0.70; // dispersion
			double d = 0.00; // skew
			for (int i = 0; i < FRAMES; i++) {
				image.clear(img);
				dendrite.draw(img, a, b, c, d);
				render.push(img);
				// a += 0.001;
				// b += 0.001;
				c += 0.001;
				d += 0.001;
			}
		}
		case "diamondsquare": {
			int size = 2;
			while (size*2 + 1 <= img->width) {
				size *= 2;
			}
			for (int i = 0; i < FRAMES; i++) {
				image.clear(img);
				diamondsquare.draw(img, size);
				render.push(img);
			}
		}
		case "lambda": {
			for (int i = 0; i < FRAMES; i++) {
				image.clear(img);
				lambda.draw(img);
				render.push(img);
			}
		}
		case "martin": {
			
			for (int i = 0; i < FRAMES; i++) {
				image.clear(img);
				martin.draw(img, 1000 + 100 * i);
				render.push(img);
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
	c->red = (int) ((double)c->red * 0.8);
	c->green = (int) ((double)c->green * 0.8);
	c->blue = (int) ((double)c->blue * 0.8);
}

double circle(double x, min, max) {
	if (x > max) {
		return min + x - max;
	}
	return x;
}
