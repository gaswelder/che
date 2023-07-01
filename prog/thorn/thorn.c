#import formats/png

const double PI = 3.141592653589793;

int main() {
    srand(time(NULL));

    const int width = 1000;
    const int height = 1000;
    double cr = (rand() % 10000) / 500.0 - 10; // -10 -> 10;
    double ci = (rand() % 10000) / 500.0 - 10;
    double xmin = -PI;
    double xmax =  PI;
    double ymin = -PI;
    double ymax =  PI;

    uint8_t *values = calloc(width*height, sizeof(uint8_t));
    for (int i = 0; i < width; i++) {
        // map i=[0..width] to zr=[xmin..xmax]
        double zr = xmin + i * (xmax - xmin) / width;
        for (int j = 0; j < height; j++) {
            // map j=[0..height] to zi=[ymin..ymax]
            double zi = ymin + j * (ymax - ymin) / height;
            values[j*width+i] = get_sample(ci, cr, zi, zr);
        }
    }

    /*
     * Render the buffer as an image.
     */
    png.png_t *img = png.png_new(width, height, png.PNG_GRAYSCALE);
    for (int j=0; j < height; j++) {
        for (int i=0; i < width; i++) {
            png.png_set_pixel(img, i, j, values[j*width+i]);
        }
    }
    png.png_save(img, "thorn.png");
    png.png_destroy(img);
	return 0;
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
