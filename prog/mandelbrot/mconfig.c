#import formats/conf

pub typedef {
	int image_width, image_height;
	double xmin, xmax;
	double ymin, ymax;
	int iterations;
	int zoom_frames;
	double zoom_rate;
	double zoomx, zoomy;
	int *prered, *pregreen, *preblue;
} config_t;

pub bool load(config_t *config, const char *path) {
	FILE *f = fopen(path, "r");
	if (!f) {
		return false;
	}

	config->image_width = 800;
	config->image_height = 600;
	config->xmin = -2.5;
	config->xmax = 1.5;
	config->ymin = -1.5;
	config->ymax = 1.5;
	config->iterations = 256;
	config->zoom_frames = 1;
	config->zoom_rate = 0.1;
	config->zoomx = -1.268794803623;
	config->zoomy = 0.353676833206;
	conf.reader_t *p = conf.new(f);
	while (conf.next(p)) {
		if (setvar(config, p->key, p->val)) {
			fprintf (stderr, "invalid key-value (%s=%s)\n", p->key, p->val);
			exit (1);
		}
	}
	conf.free(p);
	fclose(f);
	return true;
}

int setvar(config_t *config, char *base, *val) {
	switch str (base) {
		case "xmin": { config->xmin = strtod(val, NULL); }
		case "xmax": { config->xmax = strtod(val, NULL); }
		case "ymin": { config->ymin = strtod(val, NULL); }
		case "ymax": { config->ymax = strtod(val, NULL); }
		case "zoomx": { config->zoomx = strtod(val, NULL); }
		case "zoomy": { config->zoomy = strtod(val, NULL); }
		case "iterations": { config->iterations = atoi(val); }
		case "image_width": { config->image_width = atoi(val); }
		case "image_height": { config->image_height = atoi(val); }
		case "zoom_frames": { config->zoom_frames = atoi(val); }
		case "zoom_rate": { config->zoom_rate = strtod(val, NULL); }
		default: { return 1; }
	}
	return 0;
}
