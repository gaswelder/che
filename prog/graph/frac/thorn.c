#import image

// Draws thorn in the given image.
pub void draw(image.image_t *img, double cr, ci) {
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
