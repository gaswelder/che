#import formats/conf

int CMAP_LEN = 8;
int cmap_edit = 0;

int _prered[] = { 0, 0,	 0,	 0,	 128, 255, 255, 255 };
int _pregreen[] = { 0, 0,	 128, 255, 128, 128, 255, 255 };
int _preblue[] = { 0, 255, 255, 128, 0,	 0,	 128, 255 };

/* The colormap */
int *red = _prered;
int *green = _pregreen;
int *blue = _preblue;

pub typedef {
	int image_width, image_height;
	double xmin, xmax;
	double ymin, ymax;
	int iterations;
	int zoom_frames;
	double zoom_rate;
	double zoomx, zoomy;
	int *prered, *pregreen, *preblue;
	int color_width;
} config_t;

pub bool load(config_t *config, const char *path) {
	FILE *f = fopen(path, "r");
	if (!f) {
		return false;
	}

	config->prered = _prered;
	config->pregreen = _pregreen;
	config->preblue = _preblue;
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
	config->color_width = 50;

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
		case "color_width": { config->color_width = atoi(val); }
		case "red": {
			red = load_array(val);
			if (red == NULL) return 1;
		}
		case "green": {
			green = load_array (val);
			if (green == NULL) return 1;
		}
		case "blue": {
			blue = load_array (val);
			if (blue == NULL) return 1;
		}
		default: { return 1; }
	}
	return 0;
}

/* Load an array for a colormap */
int *load_array (char *list)
{
	int *c = calloc (CMAP_LEN, sizeof (int));
	int ci = 0;

	char *ptr = list;
	while (*ptr != '{' && *ptr != 0) ptr++;
	if (*ptr == 0) return NULL;

	ptr++;
	while (*ptr != 0) {
		if (*ptr == '}') break;

		c[ci] = atoi (ptr);
		ci++;
		if (ci > CMAP_LEN) {
			/* Badly formed colormap */
			if (cmap_edit) return NULL;

			CMAP_LEN *= 2;
			c = realloc (c, CMAP_LEN);
			if (c == NULL) {
				fprintf (stderr, "cmap realloc failed - %s\n", strerror (errno));
				exit (1);
			}
		}

		while (*ptr != ',' && *ptr != 0) ptr++;
		ptr++;
	}

	/* Make sure we read in enough values. */
	if (cmap_edit && CMAP_LEN != ci) return NULL;

	CMAP_LEN = ci;
	cmap_edit = 1;
	return c;
}
