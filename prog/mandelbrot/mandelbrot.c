// The original is at https://github.com/skeeto/mandelbrot

#import formats/bmp
#import image
#import mconfig.c
#import opt
#import os/threads
#import complex

// log(2)
const double logtwo = 0.693147180559945;

typedef { double xmin, xmax, ymin, ymax; } area_t;
typedef { area_t area; int frame; } job_t;

mconfig.config_t config = {};
int _prered[] = { 0, 0,	 0,	 0,	 128, 255, 255, 255 };
int _pregreen[] = { 0, 0,	 128, 255, 128, 128, 255, 255 };
int _preblue[] = { 0, 255, 255, 128, 0,	 0,	 128, 255 };

int CMAP_LEN = 8;

/* The colormap */
int *red = _prered;
int *green = _pregreen;
int *blue = _preblue;

int main (int argc, char **argv) {
	bool flag_colormap = false;
	char *confpath = "example.conf";
	opt.nargs(0, "");
	opt.str("c", "config file", &confpath);
	opt.flag("m", "create a colormap image", &flag_colormap);
	opt.parse(argc, argv);

	if (!mconfig.load(&config, confpath)) {
		fprintf(stderr, "failed to load config from %s: %s\n", confpath, strerror(errno));
		return 1;
	}
	if (flag_colormap) {
		write_colormap("cmap.bmp");
		return 0;
	}

	// 1 worker will read and render images.
	threads.pipe_t *render_in = threads.newpipe();
	threads.thr_t *render = threads.start(renderfunc, render_in);

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
		// The next frame space [center-hwidth/(1+zoom_rate), center+hwidth/(1+zoom_rate)]
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
		draw(img, a.xmin, a.xmax, a.ymin, a.ymax, config.iterations);
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

void draw(image.image_t *img, double xmin, xmax, ymin, ymax, int it) {
	int width = img->width;
	int height = img->height;
	double xres = (xmax - xmin) / width;
	double yres = (ymax - ymin) / height;
	for (int j = 0; j < height; j++) {
		for (int i = 0; i < width; i++) {
			complex.t c = {
				.re = xmin + i * xres,
				.im = ymin + j * yres
			};
			double v = get_val(c, it);
			*image.getpixel(img, i, j) = colormap(v);
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

image.rgba_t colormap(double val) {
	image.rgba_t color;

	int map_len = CMAP_LEN;
	if (val < config.color_width) {
		color.red = red[1] - (red[1] - red[0]) * (config.color_width - val) / config.color_width;
		color.green = green[1] - (green[1] - green[0]) * (config.color_width - val) / config.color_width;
		color.blue = blue[1] - (blue[1] - blue[0]) * (config.color_width - val) / config.color_width;
		return color;
	}

	val -= config.color_width;
	int base = ((int) val / config.color_width % (map_len - 1)) + 1;
	double perc = (val - (config.color_width * (int) (val / config.color_width))) / config.color_width;
	int top = base + 1;
	if (top >= map_len) {
		top = 1;
	}

	color.red = (int)((double)red[base] + (double)(red[top] - red[base]) * perc);
	color.green = (int)( (double)green[base] + (double)(green[top] - green[base]) * perc );
	color.blue = (int)( (double)blue[base] + (double)(blue[top] - blue[base]) * perc);

	return color;
}

void write_colormap(const char *filename) {
	image.image_t *img = image.new(config.color_width * CMAP_LEN * 1.5, 20);
	for (int x = 0; x < img->width; x++) {
		image.rgba_t c = colormap(x);
		for (int y = 0; y < img->height; y++) {
			*image.getpixel(img, x, y) = c;
		}
	}
	if (!bmp.write(img, filename)) {
		panic("failed to write bmp: %s", strerror(errno));
	}
	image.free(img);
}
