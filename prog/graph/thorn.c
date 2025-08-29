#import render.c
#import image
#import opt
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
		thorn(img, cr, ci);
		render.push(img);
	}
	render.end();
	image.free(img);
	return 0;
}

// Draws thorn in the given image.
void thorn(image.image_t *img, double cr, ci) {
    double xmin = -M_PI;
    double xmax =  M_PI;
    double ymin = -M_PI;
    double ymax =  M_PI;
	int width = img->width;
	int height = img->height;
    for (int i = 0; i < width; i++) {
        // map i=[0..width] to zr=[xmin..xmax]
        double zr = xmin + i * (xmax - xmin) / width;
        for (int j = 0; j < height; j++) {
            // map j=[0..height] to zi=[ymin..ymax]
            double zi = ymin + j * (ymax - ymin) / height;
			double val = get_sample(ci, cr, zi, zr);
			int px = (int) (val * 100);
			image.set(img, i, j, image.gray(px));
        }
    }
}

uint8_t get_sample(double ci, cr, zi, zr) {
    double ir = zr;
    double ii = zi;
    for (uint8_t k = 0; k < 255; k++) {
        double a = ir;
        double b = ii;
        ir = a / cos(b) + cr;
        ii = b / sin(a) + ci;
        if (ir*ir + ii*ii > 10000) {
            return k;
        }
    }
    return 255;
}
