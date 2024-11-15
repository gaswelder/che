#import formats/png
#import image
#import opt

const double PI = 3.141592653589793;

int main(int argc, char *argv[]) {
	char *size = "1000x1000";
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
	thorn(img);
	png.writeimg(img, "thorn.png");
	image.free(img);
	return 0;
}

// Draws thorn in the given image.
void thorn(image.image_t *img) {
	srand(time(NULL));
	double cr = (rand() % 10000) / 500.0 - 10; // -10 -> 10;
    double ci = (rand() % 10000) / 500.0 - 10;
    double xmin = -PI;
    double xmax =  PI;
    double ymin = -PI;
    double ymax =  PI;
	int width = img->width;
	int height = img->height;
	image.rgb_t rgb;
    for (int i = 0; i < width; i++) {
        // map i=[0..width] to zr=[xmin..xmax]
        double zr = xmin + i * (xmax - xmin) / width;
        for (int j = 0; j < height; j++) {
            // map j=[0..height] to zi=[ymin..ymax]
            double zi = ymin + j * (ymax - ymin) / height;
			double val = get_sample(ci, cr, zi, zr);
			rgb.red = (int) val;
			rgb.green = (int) val;
			rgb.blue = (int) val;
			image.set(img, i, j, rgb);
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
