#import formats/png
#import image

const int W = 250;
const int H = 200;

int main() {
    palette();
    rgba();
	rgb();
    gray();
    grayalpha();
    graystream();
    return 0;
}

void palette() {
	image.rgba_t colors[] = {
		{0, 0, 0xff, 0xff},
		{0, 0xff, 0, 0x80},
		{0xff, 0, 0, 0xff},
		{0xff, 0, 0xff, 0x80}
	};

	image.image_t *img = image.new(W, H);
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
			image.rgba_t c = colors[(x%16)/4];
			image.set(img, x, y, c);
        }
    }
	png.write(img, "test_palette.png", png.PNG_PALETTE);
	image.free(img);
}

void rgba() {
	image.image_t *img = image.new(W, H);
	image.rgba_t c = {};
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
			c.red = x & 255;
			c.green = y & 255;
			c.blue = 128;
			c.transparency = (255 - ((x / 2) & 255));
			image.set(img, x, y, c);
        }
    }
	png.write(img, "test_rgba.png", png.PNG_RGBA);
	image.free(img);
}

void rgb() {
	image.image_t *img = image.new(W, H);
	image.rgba_t c = {};

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
			c.red = x;
			c.green = y;
			c.blue = 128;
			image.set(img, x, y, c);
        }
    }

	png.write(img, "test_rgb.png", png.PNG_RGB);
	image.free(img);
}

void gray() {
	image.image_t *img = image.new(W, H);
	image.rgba_t c = {};
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
			int gray = (((x + y) / 2) & 255);
			c.red = gray;
			c.green = gray;
			c.blue = gray;
			image.set(img, x, y, c);
        }
    }
	png.write(img, "test_gray.png", png.PNG_GRAYSCALE);
	image.free(img);
}

void grayalpha() {
	image.image_t *img = image.new(W, H);
	image.rgba_t c = {};
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
			int gray = ((x + y) / 2) & 255;
			int alpha = 255 - ((y / 2) & 255);

			c.red = gray;
			c.green = gray;
			c.blue = gray;
			c.transparency = alpha;

			image.set(img, x, y, c);
        }
    }
	png.write(img, "test_gray_alpha.png", png.PNG_GRAYSCALE_ALPHA);
	image.free(img);
}

void graystream() {
	image.image_t *img = image.new(W, H);
	image.rgba_t c = {};

	int n = 0;
	for (int y = 0; y < H; y++) {
		for (int x = 0; x < W; x++) {
			int gray = 255 * (double) n / (double) (W * H);
			c.red = gray;
			c.green = gray;
			c.blue = gray;
			image.set(img, x, y, c);
			n++;
		}
	}

	png.write(img, "test_gray_stream.png", png.PNG_GRAYSCALE);
	image.free(img);
}
