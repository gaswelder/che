#import image
#import opt
#import render.c
#import att/pickover.c

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
		pickover.draw(img, i, FRAMES);
		render.push(img);
    }
	render.end();
    return 0;
}

void fade(image.rgba_t *c) {
	c->red /= 2;
	c->green /= 2;
	c->blue /= 2;
}
