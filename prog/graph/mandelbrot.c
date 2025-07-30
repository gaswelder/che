// The original is at https://github.com/skeeto/mandelbrot

#import complex
#import formats/bmp
#import formats/conf
#import formats/ppm
#import image
#import opt
#import os/threads

// log(2)
const double logtwo = 0.693147180559945;

typedef { double xmin, xmax, ymin, ymax; } area_t;
typedef { area_t area; int frame; } job_t;

typedef {
	int image_width, image_height;
	double xmin, xmax;
	double ymin, ymax;
	int iterations;
	int zoom_frames;
	double zoom_rate;
	double zoomx, zoomy;
	int *prered, *pregreen, *preblue;
} config_t;

config_t config = {
	.image_width = 800,
	.image_height = 600,
	.xmin = -2.5,
	.xmax = 1.5,
	.ymin = -1.5,
	.ymax = 1.5,
	.iterations = 256,
	.zoom_frames = 1,
	.zoom_rate = 0.1,
	.zoomx = -1.268794803623,
	.zoomy = 0.353676833206,
};

image.colormap_t *GLOBAL_CM = NULL;

pub int run(int argc, char **argv) {
	bool flag_colormap = false;
	bool render_out = false;
	char *confpath = "mandelbrot.conf";
	opt.nargs(0, "");
	opt.str("c", "config file", &confpath);
	opt.flag("m", "create a colormap image", &flag_colormap);
	opt.flag("o", "render as ppm to stdout", &render_out);
	opt.parse(argc, argv);

	if (!loadconfig(&config, confpath)) {
		fprintf(stderr, "failed to load config from %s: %s\n", confpath, strerror(errno));
		return 1;
	}

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
	GLOBAL_CM = cm;

	if (flag_colormap) {
		write_colormap(cm, "cmap.bmp");
		return 0;
	}

	// 1 worker will read and render images.
	threads.pipe_t *render_in = threads.newpipe();
	threads.thr_t *render = NULL;
	if (render_out) {
		render = threads.start(renderfunc_stdout, render_in);
	} else {
		render = threads.start(renderfunc, render_in);
	}

	// 8 workers will consume parameters and write to the renderer.
	threads.pipe_t *worker_in = threads.newpipe();
	threads.thr_t **tt = calloc!(8, sizeof(threads.thr_t *));
	worker_arg_t *aa = calloc!(8, sizeof(worker_arg_t));
	for (int i = 0; i < 8; i++) {
		aa[i].in = worker_in;
		aa[i].out = render_in;
		tt[i] = threads.start(workerfunc, &aa[i]);
	}

	//
	// Send jobs to the queue.
	//
	double hw = (config.xmax - config.xmin) / 2;
	double hh = (config.ymax - config.ymin) / 2;
	job_t *jobs = calloc!(config.zoom_frames, sizeof(job_t));
	for (int i = 0; i < config.zoom_frames; i++) {
		job_t *job = &jobs[i];
		// The first frame spans [center-hwidth, center+hwidth]
		// The next frame spans [center-hwidth/(1+zoom_rate), center+hwidth/(1+zoom_rate)]
		job->frame = i;
		job->area.xmin = config.zoomx - hw;
		job->area.xmax = config.zoomx + hw;
		job->area.ymin = config.zoomy - hh;
		job->area.ymax = config.zoomy + hh;
		hw /= (1 + config.zoom_rate);
		hh /= (1 + config.zoom_rate);
		// iterations *= (1 + zoom_rate * 0.25 * frame);

		threads.pwrite(worker_in, job);
	}

	// Wait for the workers to finish.
	threads.pclose(worker_in);
	for (int i = 0; i < 8; i++) {
		threads.wait(tt[i], NULL);
	}
	threads.freepipe(worker_in);

	// Wait for the render to finish.
	threads.pclose(render_in);
	threads.wait(render, NULL);
	threads.freepipe(render_in);

	free(jobs);
	free(tt);
	return 0;
}

typedef { threads.pipe_t *in, *out; } worker_arg_t;

void *workerfunc(void *arg0) {
	worker_arg_t *arg = arg0;
	threads.pipe_t *in = arg->in;
	threads.pipe_t *out = arg->out;
	void *buf[1] = {};
	while (threads.pread(in, buf)) {
		job_t *job = buf[0];
		area_t a = job->area;
		image.image_t *img = image.new(config.image_width, config.image_height);
		draw(img, GLOBAL_CM, a, config.iterations);
		render_val_t *v = calloc!(1, sizeof(render_val_t));
		v->img = img;
		v->frame = job->frame;
		threads.pwrite(out, v);
	}
	return NULL;
}

typedef {
	image.image_t *img;
	int frame;
} render_val_t;

void *renderfunc(void *arg) {
	threads.pipe_t *in = arg;
	char filename[100] = {};
	void *buf[1] = {};
	while (threads.pread(in, buf)) {
		render_val_t *val = buf[0];
		snprintf(filename, sizeof(filename), "dump/%03d.bmp", val->frame);
		if (!bmp.write(val->img, filename)) {
			panic("failed to write bmp: %s", strerror(errno));
		}
		fprintf(stderr, "%s\n", filename);
		image.free(val->img);
		free(val);
	}
	return NULL;
}

void *renderfunc_stdout(void *arg) {
	threads.pipe_t *in = arg;
	void *buf[1] = {};
	while (threads.pread(in, buf)) {
		render_val_t *val = buf[0];
		ppm.writeimg(val->img, stdout);
		image.free(val->img);
		free(val);
	}
	return NULL;
}

void draw(image.image_t *img, image.colormap_t *cm, area_t area, int it) {
	int width = img->width;
	int height = img->height;
	double xres = (area.xmax - area.xmin) / width;
	double yres = (area.ymax - area.ymin) / height;
	for (int j = 0; j < height; j++) {
		for (int i = 0; i < width; i++) {
			complex.t c = {
				.re = area.xmin + i * xres,
				.im = area.ymin + j * yres
			};
			double v = get_val(c, it);
			*image.getpixel(img, i, j) = image.mapcolor(cm, v);
		}
	}
}

// The Mandelbrot set is based on the function
//
//      f(z, c) = z^2 + c.
//
// The variable c is reinterpreted as a pixel coordinate: x=Re(c), y=Im(c).
// For each sample point c we're looking how quickly the sequence
//
//      |f(0, c)|, |f(0, f(0, c))|, ...
//
// goes to infinity. Pixels may be colored then according to how soon the
// sequence crosses a chosen threshold. The threshold should be higher than 2
// (which is max(abs(f(any, any)))).

// * If c is held constant and the initial value of z is varied instead,
// we get a Julia set instead.

double get_val(complex.t c, int it) {
	complex.t z = { 0, 0 };

	int count = 0;
	while (complex.abs2(z) < 4 && count < it) {
		/* Z = Z^2 + C */
		z = complex.sum( complex.mul(z, z), c );
		count++;
	}
	if (complex.abs2(z) < 4) {
		return 0;
	}
	return smooth(count, complex.abs(z));
}

double smooth(int count, double amp) {
	double off = (double) count + 1;
	return off - log(log(amp)) / logtwo;
}

void write_colormap(image.colormap_t *cm, const char *filename) {
	image.image_t *img = image.new(cm->color_width * cm->size * 1.5, 20);
	for (int x = 0; x < img->width; x++) {
		image.rgba_t c = image.mapcolor(cm, x);
		for (int y = 0; y < img->height; y++) {
			*image.getpixel(img, x, y) = c;
		}
	}
	if (!bmp.write(img, filename)) {
		panic("failed to write bmp: %s", strerror(errno));
	}
	image.free(img);
}


bool loadconfig(config_t *c, const char *path) {
	FILE *f = fopen(path, "r");
	if (!f) {
		return false;
	}
	conf.reader_t *p = conf.new(f);
	while (conf.next(p)) {
		char *base = p->key;
		char *val = p->val;
		switch str (base) {
			case "xmin": { c->xmin = strtod(val, NULL); }
			case "xmax": { c->xmax = strtod(val, NULL); }
			case "ymin": { c->ymin = strtod(val, NULL); }
			case "ymax": { c->ymax = strtod(val, NULL); }
			case "zoomx": { c->zoomx = strtod(val, NULL); }
			case "zoomy": { c->zoomy = strtod(val, NULL); }
			case "iterations": { c->iterations = atoi(val); }
			case "image_width": { c->image_width = atoi(val); }
			case "image_height": { c->image_height = atoi(val); }
			case "zoom_frames": { c->zoom_frames = atoi(val); }
			case "zoom_rate": { c->zoom_rate = strtod(val, NULL); }
			default: {
				fprintf(stderr, "invalid config value: %s=%s\n", p->key, p->val);
				exit(1);
			}
		}
	}
	conf.free(p);
	fclose(f);
	return true;
}
