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

model_t mdejong   = {.name = "dejong",       .newparams = dejong.newparams,       .draw = dejong.draw,       .mutate = dejong.mutateparams};
model_t mhenon    = {.name = "henon",        .newparams = henon.newparams,        .draw = henon.draw,        .mutate = henon.mutateparams};
model_t mikeda    = {.name = "ikeda",        .newparams = ikeda.newparams,        .draw = ikeda.draw,        .mutate = ikeda.mutateparams};
model_t mmandelbrot = {.name = "mandelbrot", .newparams = mandelbrot.newparams,   .draw = mandelbrot.draw,   .mutate = mandelbrot.mutateparams};
model_t mpickover = {.name = "pickover",     .newparams = pickover.newparams,     .draw = pickover.draw,     .mutate = pickover.mutateparams};
model_t mdendrite = {.name = "dendrite",     .newparams = dendrite.newparams,     .draw = dendrite.draw,     .mutate = dendrite.mutateparams};
model_t mdiamondsquare = {.name = "diamondsquare", .newparams = diamondsquare.newparams, .draw = diamondsquare.draw, .mutate = diamondsquare.mutateparams};
model_t mdynamic  = {.name = "dynamic",      .newparams = dynamic.newparams,      .draw = dynamic.draw,      .mutate = dynamic.mutateparams};
model_t mfrothy   = {.name = "frothy",       .newparams = frothy.newparams,       .draw = frothy.draw,       .mutate = frothy.mutateparams};
model_t mgingerbread = {.name = "gingerbread", .newparams = gingerbread.newparams, .draw = gingerbread.draw, .mutate = gingerbread.mutateparams};
model_t mlambda   = {.name = "lambda",       .newparams = lambda.newparams,       .draw = lambda.draw,       .mutate = lambda.mutateparams};
model_t mmartin   = {.name = "martin",       .newparams = martin.newparams,       .draw = martin.draw,       .mutate = martin.mutateparams};
model_t mthorn    = {.name = "thorn",        .newparams = thorn.newparams,        .draw = thorn.draw,        .mutate = thorn.mutateparams};

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
			model_t m = mthorn;
			void *p = m.newparams();
			for (int i = 0; i < FRAMES; i++) {
				m.draw(img, p);
				render.push(img);
				m.mutate(p);
			}
			free(p);
		}
		case "pickover": {
			model_t m = mpickover;
			void *p = m.newparams();
			for (int i = 0; i < FRAMES; i++) {
				image.apply(img, fade);
				m.draw(img, p);
				render.push(img);
				m.mutate(p);
			}
			free(p);
		}
		case "dejong": {
			model_t m = mdejong;
			void *p = m.newparams();
			for (int i = 0; i < FRAMES; i++) {
				m.draw(img, p);
				render.push(img);
				image.apply(img, fade);
				m.mutate(p);
			}
			free(p);
		}
		case "ikeda": {
			model_t m = mikeda;
			void *p = m.newparams();
			for (int i = 0; i < FRAMES; i++) {
				image.clear(img);
				m.draw(img, p);
				render.push(img);
				m.mutate(p);
			}
			free(p);
		}
		case "mandelbrot": {
			model_t m = mmandelbrot;
			void *p = m.newparams();
			for (int i = 0; i < FRAMES; i++) {
				fprintf(stderr, "%d / %d\n", i, FRAMES);
				m.draw(img, p);
				render.push(img);
				m.mutate(p);
			}
			free(p);
		}
		case "frothy": {
			model_t m = mfrothy;
			void *p = m.newparams();
			for (int i = 0; i < FRAMES; i++) {
				image.clear(img);
				m.draw(img, p);
				render.push(img);
				m.mutate(p);
			}
			free(p);
		}
		case "henon": {
			model_t m = mhenon;
			void *p = m.newparams();
			for (int i = 0; i < FRAMES; i++) {
				image.clear(img);
				m.draw(img, p);
				render.push(img);
				m.mutate(p);
			}
			free(p);
		}
		case "gingerbread": {
			model_t m = mgingerbread;
			void *p = m.newparams();
			for (int i = 0; i < FRAMES; i++) {
				image.clear(img);
				m.draw(img, p);
				render.push(img);
				m.mutate(p);
			}
			free(p);
		}
		case "dendrite": {
			model_t m = mdendrite;
			void *p = m.newparams();
			for (int i = 0; i < FRAMES; i++) {
				image.clear(img);
				m.draw(img, p);
				render.push(img);
				m.mutate(p);
			}
			free(p);
		}
		case "diamondsquare": {
			model_t m = mdiamondsquare;
			void *p = m.newparams();
			for (int i = 0; i < FRAMES; i++) {
				image.clear(img);
				m.draw(img, p);
				render.push(img);
				m.mutate(p);
			}
			free(p);
		}
		case "lambda": {
			model_t m = mlambda;
			void *p = m.newparams();
			for (int i = 0; i < FRAMES; i++) {
				image.clear(img);
				m.draw(img, p);
				render.push(img);
				m.mutate(p);
			}
			free(p);
		}
		case "martin": {
			model_t m = mmartin;
			void *p = m.newparams();
			for (int i = 0; i < FRAMES; i++) {
				image.clear(img);
				m.draw(img, p);
				render.push(img);
				m.mutate(p);
			}
			free(p);
		}
		case "dynamic": {
			model_t m = mdynamic;
			void *p = m.newparams();
			for (int i = 0; i < FRAMES; i++) {
				image.clear(img);
				m.draw(img, p);
				render.push(img);
				m.mutate(p);
			}
			free(p);
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
