#import formats/jpg
#import formats/png
#import image

int main(int argc, char *argv[]) {
	if (argc == 1) {
		puts("drawn basis to 1.png");
		drawbasis();
		return 0;
	}
    jpg.jpeg_t *j = jpg.read(argv[1]);
	png.write(j->img, "1.png", png.PNG_RGB);
	jpg.free(j);
	return 0;
}

void drawbasis() {
	image.image_t *img = image.new(64 + 9, 64 + 9);
    image.rgba_t red = {255, 0, 0, 0};
    image.fill(img, red);
    for (int u = 0; u < 8; u++) {
        for (int v = 0; v < 8; v++) {
            square(img, u, v);
        }
    }
    image.image_t *img2 = image.explode(img, 10);
    image.free(img);

    png.write(img2, "1.png", png.PNG_RGB);
    image.free(img2);
}

void square(image.image_t *img, int u, v) {
    int x0 = 1;
    for (int i = 0; i < u; i++) {
        x0 += 1;
        x0 += 8;
    }
    int y0 = 1;
    for (int i = 0; i < v; i++) {
        y0 += 1;
        y0 += 8;
    }
    double shape[64];
    jpg.getshape(shape, u, v);
    for (int x = 0; x < 8; x++) {
        for (int y = 0; y < 8; y++) {
            double val = shape[x*8+y];
            val += 1;
            val /= 2; // 0..1
            image.set(img, x + x0, y + y0, image.gray((int) (val * 255)));
        }
    }
}
