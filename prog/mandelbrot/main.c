// The original is at https://github.com/skeeto/mandelbrot

#import formats/bmp
#import formats/conf
#import mandel.c
#import opt
#import proc.c

typedef {
	int red;
	int green;
	int blue;
} Rgb;

typedef { int w, h; } dim_t;
typedef { double xmin, xmax, ymin, ymax; } area_t;
typedef { area_t area; int frame; } job_t;

char *version = "0.1-alpha";

bool verbose = true;

// -----------------
// config parameters
// -----------------
dim_t image_size = { 800, 600 };
double xmin = -2.5;
double xmax = 1.5;
double ymin = -1.5;
double ymax = 1.5;
int iterations = 256;
int zoom_frames = 1;
double zoom_rate = 0.1;
double zoomx = -1.268794803623;
double zoomy = 0.353676833206;
int prered[]	 = { 0, 0,	 0,	 0,	 128, 255, 255, 255 };
int pregreen[] = { 0, 0,	 128, 255, 128, 128, 255, 255 };
int preblue[]	= { 0, 255, 255, 128, 0,	 0,	 128, 255 };
// -------------

int write_text = 0;
int cmap_len = 8;
int cmap_edit = 0;
int cwidth = 50;

/* The colormap */
int *red = prered;
int *green = pregreen;
int *blue = preblue;

void verbprint(const char *format, ...) {
	if (!verbose) return;
	va_list args = {};
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}

int main (int argc, char **argv) {
	bool flag_colormap = false;
	bool flag_quiet = false;
	bool flag_v = false;
	bool flag_help = false;
	char *confpath = "example.conf";

	opt.str("c", "config file", &confpath);
	opt.opt_bool("m", "create a colormap image", &flag_colormap);
	opt.opt_bool("q", "quiet mode", &flag_quiet);
	opt.opt_bool("h", "help", &flag_help);
	opt.opt_bool("v", "version", &flag_v);
	char **args = opt.opt_parse(argc, argv);
	if (*args) {
		fprintf(stderr, "this program only has options, got %s\n", *args);
		return 1;
	}
	if (flag_help) exit(opt.usage());
	if (flag_v) {
		verbprint("mandelgen, version %s.\n", version);
		verbprint("Copyright (C) 2007 Chris Wellons\n");
		verbprint("This is free software; see the source code ");
		verbprint("for copying conditions.\n");
		verbprint("There is ABSOLUTELY NO WARRANTY; not even for ");
		verbprint("MERCHANTIBILITY or\nFITNESS FOR A PARTICULAR PURPOSE.\n\n");
	}
	if (flag_quiet) verbose = false;
	if (flag_colormap) {
		verbprint("written colormap to cmap.bmp\n");
		write_colormap("cmap.bmp");
		return 0;
	}

	FILE *infile = fopen (confpath, "r");
	if (!infile) {
		fprintf(stderr, "failed to open %s: %s\n", confpath, strerror(errno));
		return 1;
	}
	conf.reader_t *p = conf.new(infile);
	while (conf.next(p)) {
		if (setvar(p->key, p->val)) {
			fprintf (stderr, "invalid key-value (%s=%s)\n", p->key, p->val);
			exit (1);
		}
	}
	conf.free(p);
	fclose(infile);

	//
	// Create the jobs table.
	//
	double hw = (xmax - xmin) / 2;
	double hh = (ymax - ymin) / 2;
	job_t *jobs = calloc(zoom_frames, sizeof(job_t));
	if (!jobs) panic("calloc failed");
	for (int frame = 0; frame < zoom_frames; frame++) {
		// The first frame spans [center-hwidth, center+hwidth]
		// The next frame space [center-hwidth/(1+zoom_rate), center+hwidth/(1+zoom_rate)]
		jobs[frame].frame = frame;
		jobs[frame].area.xmin = zoomx - hw;
		jobs[frame].area.xmax = zoomx + hw;
		jobs[frame].area.ymin = zoomy - hh;
		jobs[frame].area.ymax = zoomy + hh;
		hw /= (1 + zoom_rate);
		hh /= (1 + zoom_rate);
		// iterations *= (1 + zoom_rate * 0.25 * frame);
	}

	//
	// Run the jobs
	//
	int nworkers = 0;
	int jobindex = 0;
	for (int i = 0; i < 8; i++) {
		if (jobindex >= zoom_frames) break;
		spawn(&jobs[jobindex++]);
		nworkers++;
	}
	while (nworkers > 0) {
		proc.wait(NULL);
		nworkers--;
		if (jobindex < zoom_frames) {
			spawn(&jobs[jobindex++]);
			nworkers++;
		}
	}
	return 0;
}

void spawn(job_t *job) {
    int pid = proc.fork();
    if (pid < 0) panic("fork failed");
    if (pid > 0) return;
    area_t *a = &job->area;
	verbprint("frame %d/%d, ", job->frame, zoom_frames);
	verbprint("x[%f, %f], y[%f, %f]\n", a->xmin, a->xmax, a->ymin, a->ymax);
	workermain(job->frame, *a, iterations);
	exit(0);
}

int workermain(int zi, area_t a, int iterations) {
	double *data = mandel.gen(image_size.w, image_size.h, a.xmin, a.xmax, a.ymin, a.ymax, iterations);
	char basename[20];
	snprintf (basename, 20, "out-%010d", zi);

	if (write_text) {
		save_values(basename, data, image_size);
	}
	
	char filename[100] = {};
	snprintf(filename, sizeof(filename), "dump/%s.bmp", basename);
	save_image(filename, data, iterations, image_size);
	fprintf(stderr, "written %s\n", filename);

	free (data);
	return 0;
}

int setvar(char *base, *val) {
	if (strcmp (base, "xmin") == 0)
		xmin = strtod (val, NULL);
	else if (strcmp (base, "xmax") == 0)
		xmax = strtod (val, NULL);
	else if (strcmp (base, "ymin") == 0)
		ymin = strtod (val, NULL);
	else if (strcmp (base, "ymax") == 0)
		ymax = strtod (val, NULL);
	else if (strcmp (base, "zoomx") == 0)
		zoomx = strtod (val, NULL);
	else if (strcmp (base, "zoomy") == 0)
		zoomy = strtod (val, NULL);
	else if (strcmp (base, "iterations") == 0)
		iterations = atoi (val);
	else if (strcmp (base, "image_width") == 0)
		image_size.w = atoi (val);
	else if (strcmp (base, "image_height") == 0)
		image_size.h = atoi (val);
	else if (strcmp (base, "zoom_frames") == 0)
		zoom_frames = atoi (val);
	else if (strcmp (base, "zoom_rate") == 0)
		zoom_rate = strtod (val, NULL);
	else if (strcmp (base, "color_width") == 0)
		cwidth = atoi (val);
	else if (strcmp (base, "red") == 0)
		{
			red = load_array (val);
			if (red == NULL)
	return 1;
		}
	else if (strcmp (base, "green") == 0)
		{
			green = load_array (val);
			if (green == NULL)
	return 1;
		}
	else if (strcmp (base, "blue") == 0)
		{
			blue = load_array (val);
			if (blue == NULL)
	return 1;
		}
	else
		return 1;

	return 0;
}

/* Load an array for a colormap */
int *load_array (char *list)
{
	int *c = calloc (cmap_len, sizeof (int));
	int ci = 0;

	char *ptr = list;
	while (*ptr != '{' && *ptr != 0) ptr++;
	if (*ptr == 0) return NULL;

	ptr++;
	while (*ptr != 0) {
		if (*ptr == '}') break;

		c[ci] = atoi (ptr);
		ci++;
		if (ci > cmap_len) {
			/* Badly formed colormap */
			if (cmap_edit) return NULL;

			cmap_len *= 2;
			c = realloc (c, cmap_len);
			if (c == NULL) {
				fprintf (stderr, "cmap realloc failed - %s\n", strerror (errno));
				exit (1);
			}
		}

		while (*ptr != ',' && *ptr != 0) ptr++;
		ptr++;
	}

	/* Make sure we read in enough values. */
	if (cmap_edit && cmap_len != ci) return NULL;

	cmap_len = ci;
	cmap_edit = 1;
	return c;
}

Rgb colormap (double val, int iterations)
{
	Rgb color;

	/* Colormap size */
	int map_len = cmap_len;

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
	if (val < cwidth)
		{
			color.red = red[1] - (red[1] - red[0]) * (cwidth - val) / cwidth;
			color.green =
	green[1] - (green[1] - green[0]) * (cwidth - val) / cwidth;
			color.blue = blue[1] - (blue[1] - blue[0]) * (cwidth - val) / cwidth;
			return color;
		}

	val -= cwidth;
	int base = ((int) val / cwidth % (map_len - 1)) + 1;
	double perc = (val - (cwidth * (int) (val / cwidth))) / cwidth;
	int top = base + 1;
	if (top >= map_len)
		top = 1;

	color.red = red[base] + (red[top] - red[base]) * perc;
	color.green = green[base] + (green[top] - green[base]) * perc;
	color.blue = blue[base] + (blue[top] - blue[base]) * perc;

// #endif
	return color;
}

void write_colormap (char *filename) {
	int w = cwidth * cmap_len * 1.5;
	int h = 20;
	bmp.t *cmap = bmp.new(w, h);
	for (int i = 0; i < w; i++) {
		Rgb rgb = colormap (i, w);
		fillRect(cmap, i, 0, i+1, h, rgb.red, rgb.green, rgb.blue);
	}
	bmp.write(cmap, filename);
	bmp.free(cmap);
}

void fillRect(bmp.t *i, int left, top, right, bottom, int red, green, blue) {
	for (int y = top; y < bottom; y++) {
		for (int x = left; x < right; x++) {
			bmp.set(i, x, y, red, green, blue);
		}
	}
}

void save_values(char *basename, double *data, dim_t size) {
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

void save_image(char *filename, double *data, int iterations, dim_t size) {
	bmp.t *img = bmp.new(size.w, size.h);
	for (int j = 0; j < size.h; j++) {
		for (int i = 0; i < size.w; i++) {
			Rgb rgb = colormap(data[i + j * size.w], iterations);
			bmp.set(img, i, j, rgb.red, rgb.green, rgb.blue);
		}
	}
	bmp.write(img, filename);
	bmp.free(img);
}
