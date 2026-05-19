#import frac/thorn.c
#import image
#import opt
#import render.c
#import rnd

pub int run(int argc, char *argv[]) {
	char *size = "400x400";
	opt.nargs(0, "");
	opt.str("s", "image size", &size);
	opt.parse(argc, argv);

	int width;
	int height;
	if (sscanf(size, "%dx%d", &width, &height) != 2) {
		fprintf(stderr, "failed to parse the size\n");
		return 1;
	}

	image.image_t *img = image.new(width, height);

	double cr = (rnd.intn(10000)) / 500.0 - 10; // -10 -> 10;
    double ci = (rnd.intn(10000)) / 500.0 - 10;

	for (int i = 0; i < 100; i++) {
		ci += 0.1;
		cr += 0.1;
		thorn.draw(img, cr, ci);
		render.push(img);
	}
	render.end();
	image.free(img);
	return 0;
}
