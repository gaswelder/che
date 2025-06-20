// The original is at https://github.com/skeeto/mandelbrot

#import formats/bmp
#import image
#import mandel.c
#import mconfig.c
#import opt
#import os/proc

typedef { double xmin, xmax, ymin, ymax; } area_t;
typedef { area_t area; int frame; } job_t;

bool verbose = false;

mconfig.config_t config = {};
int _prered[] = { 0, 0,	 0,	 0,	 128, 255, 255, 255 };
int _pregreen[] = { 0, 0,	 128, 255, 128, 128, 255, 255 };
int _preblue[] = { 0, 255, 255, 128, 0,	 0,	 128, 255 };

int write_text = 0;
int CMAP_LEN = 8;


/* The colormap */
int *red = _prered;
int *green = _pregreen;
int *blue = _preblue;

void verbprint(const char *format, ...) {
	if (!verbose) return;
	va_list args = {};
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}

int main (int argc, char **argv) {
	bool flag_colormap = false;
	char *confpath = "example.conf";
	opt.nargs(0, "");
	opt.str("c", "config file", &confpath);
	opt.flag("m", "create a colormap image", &flag_colormap);
	opt.flag("v", "verbose mode", &verbose);
	opt.parse(argc, argv);

	if (!mconfig.load(&config, confpath)) {
		fprintf(stderr, "failed to load config from %s: %s\n", confpath, strerror(errno));
		return 1;
	}
	if (flag_colormap) {
		verbprint("written colormap to cmap.bmp\n");
		write_colormap("cmap.bmp");
		return 0;
	}

	//
	// Create the jobs table.
	//
	double hw = (config.xmax - config.xmin) / 2;
	double hh = (config.ymax - config.ymin) / 2;
	job_t *jobs = calloc(config.zoom_frames, sizeof(job_t));
	if (!jobs) panic("calloc failed");
	for (int frame = 0; frame < config.zoom_frames; frame++) {
		// The first frame spans [center-hwidth, center+hwidth]
		// The next frame space [center-hwidth/(1+zoom_rate), center+hwidth/(1+zoom_rate)]
		jobs[frame].frame = frame;
		jobs[frame].area.xmin = config.zoomx - hw;
		jobs[frame].area.xmax = config.zoomx + hw;
		jobs[frame].area.ymin = config.zoomy - hh;
		jobs[frame].area.ymax = config.zoomy + hh;
		hw /= (1 + config.zoom_rate);
		hh /= (1 + config.zoom_rate);
		// iterations *= (1 + zoom_rate * 0.25 * frame);
	}

	void **clip = calloc(config.zoom_frames + 1, sizeof(&jobs[0]));
	for (int i = 0; i < config.zoom_frames; i++) {
		clip[i] = &jobs[i];
	}
	proc.parallel(&workerfunc, clip);
	free(clip);
	return 0;
}

int workerfunc(void *arg) {
	job_t *job = arg;
	area_t a = job->area;
	verbprint("frame %d/%d, ", job->frame, config.zoom_frames);
	verbprint("x[%f, %f], y[%f, %f]\n", a.xmin, a.xmax, a.ymin, a.ymax);

	double *data = mandel.gen(config.image_width, config.image_height, a.xmin, a.xmax, a.ymin, a.ymax, config.iterations);
	char basename[20];
	snprintf (basename, 20, "out-%010d", job->frame);

	image.dim_t image_size = { config.image_width, config.image_height };

	if (write_text) {
		save_values(basename, data, image_size);
	}
	
	char filename[100] = {};
	snprintf(filename, sizeof(filename), "dump/%s.bmp", basename);
	save_image(filename, data, config.iterations, image_size);
	fprintf(stderr, "written %s\n", filename);

	free (data);
	return 0;
}

image.rgba_t colormap(double val, int iterations) {
	image.rgba_t color;

	/* Colormap size */
	int map_len = CMAP_LEN;

// #if FLAT_CMAP
//	 map_len--;
//	 int trans = map_len - 1;

//	 /* Base transition */
//	 int base = val / iterations * trans;
//	 if (base >= map_len)
//		 {
//			 base = map_len - 1;
//			 val = iterations;
//		 }

//	 /* Interpolate */
//	 color.red = red[base]
//		 + (val - (iterations / trans) * base) / (iterations / trans)
//		 * (red[base + 1] - red[base]);

//	 color.green = green[base]
//		 + (val - (iterations / trans) * base) / (iterations / trans)
//		 * (green[base + 1] - green[base]);

//	 color.blue = blue[base]
//		 + (val - (iterations / trans) * base) / (iterations / trans)
//		 * (blue[base + 1] - blue[base]);
// #else
	(void) iterations;
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

// #endif
	return color;
}

void write_colormap(const char *filename) {
	image.image_t *img = image.new(config.color_width * CMAP_LEN * 1.5, 20);
	for (int x = 0; x < img->width; x++) {
		image.rgba_t c = colormap(x, img->width);
		for (int y = 0; y < img->height; y++) {
			*image.getpixel(img, x, y) = c;
		}
	}
	if (!bmp.write(img, filename)) {
		panic("failed to write bmp: %s", strerror(errno));
	}
	image.free(img);
}

void save_values(char *basename, double *data, image.dim_t size) {
	char filename[100] = {};
	snprintf (filename, sizeof(filename), "%s.txt", basename);
	FILE *fout = fopen (filename, "w");
	for (int j = 0; j < size.h; j++) {
		for (int i = 0; i < size.w; i++)
			fprintf (fout, "%f", data[i + j * size.w]);
		fprintf (fout, "\n");
	}
	fclose (fout);
}

void save_image(const char *filename, double *data, int iterations, image.dim_t size) {
	image.image_t *img = image.new(size.w, size.h);
	for (int j = 0; j < size.h; j++) {
		for (int i = 0; i < size.w; i++) {
			image.rgba_t c = colormap(data[i + j * size.w], iterations);
			*image.getpixel(img, i, j) = c;
		}
	}
	if (!bmp.write(img, filename)) {
		panic("failed to write bmp: %s", strerror(errno));
	}
	image.free(img);
}
