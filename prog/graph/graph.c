#import att/dejong.c
#import att/henon.c
#import att/ikeda.c
#import att/pickover.c
#import frac/dendrite.c
#import frac/diamondsquare.c
#import frac/dynamic.c
#import frac/frothy.c
#import frac/gingerbread.c
#import frac/lambda.c
#import frac/mandelbrot.c
#import frac/martin.c
#import frac/thorn.c
#import image
#import opt
#import render.c

typedef void *newparams_func_t();
typedef void draw_func_t(image.image_t *, void *);
typedef void mutate_func_t(void *);

typedef {
	const char *name;
	newparams_func_t *newparams;
	draw_func_t *draw;
	mutate_func_t *mutate;
} model_t;

model_t models[] = {
	{.name = "dejong",       .newparams = dejong.newparams,       .draw = dejong.draw,       .mutate = dejong.mutateparams},
	{.name = "henon",        .newparams = henon.newparams,        .draw = henon.draw,        .mutate = henon.mutateparams},
	{.name = "ikeda",        .newparams = ikeda.newparams,        .draw = ikeda.draw,        .mutate = ikeda.mutateparams},
	{.name = "mandelbrot", .newparams = mandelbrot.newparams,   .draw = mandelbrot.draw,   .mutate = mandelbrot.mutateparams},
	{.name = "pickover",     .newparams = pickover.newparams,     .draw = pickover.draw,     .mutate = pickover.mutateparams},
	{.name = "dendrite",     .newparams = dendrite.newparams,     .draw = dendrite.draw,     .mutate = dendrite.mutateparams},
	{.name = "diamondsquare", .newparams = diamondsquare.newparams, .draw = diamondsquare.draw, .mutate = diamondsquare.mutateparams},
	{.name = "dynamic",      .newparams = dynamic.newparams,      .draw = dynamic.draw,      .mutate = dynamic.mutateparams},
	{.name = "frothy",       .newparams = frothy.newparams,       .draw = frothy.draw,       .mutate = frothy.mutateparams},
 	{.name = "gingerbread", .newparams = gingerbread.newparams, .draw = gingerbread.draw, .mutate = gingerbread.mutateparams},
	{.name = "lambda",       .newparams = lambda.newparams,       .draw = lambda.draw,       .mutate = lambda.mutateparams},
	{.name = "martin",       .newparams = martin.newparams,       .draw = martin.draw,       .mutate = martin.mutateparams},
	{.name = "thorn",        .newparams = thorn.newparams,        .draw = thorn.draw,        .mutate = thorn.mutateparams},
};

int main(int argc, char *argv[]) {
	char *size = "400x400";
	bool ffade = false;
	opt.nargs(1, "<algorithm = thorn / pickover / dejong / ikeda / mandelbrot / ...>");
	opt.str("s", "image size", &size);
	opt.flag("f", "fade effect", &ffade);
	char **args = opt.parse(argc, argv);

	int width;
	int height;
	if (sscanf(size, "%dx%d", &width, &height) != 2) {
		fprintf(stderr, "failed to parse the size\n");
		return 1;
	}

	const int FRAMES = 1000;

	image.image_t *img = image.new(width, height);

	bool found = false;
	model_t m = {};
	for (size_t i = 0; i < nelem(models); i++) {
		if (strcmp(models[i].name, args[0]) == 0) {
			found = true;
			m = models[i];
			break;
		}
	}
	if (!found) {
		fprintf(stderr, "unknown model: %s\n", args[0]);
		return 1;
	}

	void *p = m.newparams();
	for (int i = 0; i < FRAMES; i++) {
		// fprintf(stderr, "%d / %d\n", i, FRAMES);
		if (ffade) {
			image.apply(img, fade);
		} else {
			image.clear(img);
		}
		m.draw(img, p);
		render.push(img);		
		m.mutate(p);
	}
	free(p);
	render.end();
	image.free(img);
	return 0;
}

void fade(image.rgba_t *c) {
	c->red = (int) ((double)c->red * 0.8);
	c->green = (int) ((double)c->green * 0.8);
	c->blue = (int) ((double)c->blue * 0.8);
}
