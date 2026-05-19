#import att/ikeda.c
#import image
#import render.c

double a = 0.85;
double b = 0.9;
double k = 0.4;
double p = 7.7;

pub int run(int argc, char **argv) {
	(void) argc;
	(void) argv;
	int width = 800;
	int height = 600;

	image.image_t *img = image.new(width, height);
	for (int i = 0; i < 100; i++) {
		image.clear(img);
		ikeda.draw(img, a, b, k, p);
		render.push(img);
		a -= 0.001;
		// b += 0.0001;
		// k += 0.001;
		p += 0.01;
	}
	image.free(img);
	return 0;
}
