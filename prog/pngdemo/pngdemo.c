#import formats/png

const int W = 250;
const int H = 200;

int RGBA(int r, g, b, a) {
    return (r) | (g << 8) | (b << 16) | (a << 24);
}

int RGB(int r, g, b) {
    return RGBA(r, g, b, 0xff);
}

int ALPHA(int c, a) {
    return (c) | ((a) << 8);
}

int main() {
    png.png_t *p = png.png_new(W, H, png.PNG_PALETTE);
    uint32_t palette[] = {
            RGBA(0, 0, 0xff, 0xff),
            RGBA(0, 0xff, 0, 0x80),
            RGBA(0xff, 0, 0, 0xff),
            RGBA(0xff, 0, 0xff, 0x80)
    };
    png.png_set_palette(p, palette, 4);

    int x;
    int y;
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            png.png_set_pixel(p, x, y, (x % 16) / 4);
        }
    }

    png.png_save(p, "test_palette.png");
    png.png_destroy(p);

    // -----------------

    p = png.png_new(W, H, png.PNG_RGBA);

    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            png.png_set_pixel(p, x, y, RGBA(x & 255, y & 255, 128, (255 - ((x / 2) & 255))));
        }
    }
    png.png_save(p, "test_rgba.png");
    png.png_destroy(p);

    // -----------------

    p = png.png_new(W, H, png.PNG_RGB);

    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            png.png_set_pixel(p, x, y, RGB(x & 255, y & 255, 128));
        }
    }
    png.png_save(p, "test_rgb.png");
    png.png_destroy(p);

    // -----------------

    p = png.png_new(W, H, png.PNG_GRAYSCALE);

    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            png.png_set_pixel(p, x, y, (((x + y) / 2) & 255));
        }
    }
    png.png_save(p, "test_gray.png");
    png.png_destroy(p);

    // -----------------

    grayalpha();
    graystream();

    return 0;
}

void grayalpha() {
	png.png_t *p = png.png_new(W, H, png.PNG_GRAYSCALE_ALPHA);

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            png.png_set_pixel(p, x, y, ALPHA(((x + y) / 2) & 255, 255 - ((y / 2) & 255)));
        }
    }
    png.png_save(p, "test_gray_alpha.png");
    png.png_destroy(p);
}

void graystream() {
	png.png_t *p = png.png_new(W, H, png.PNG_GRAYSCALE);

	int n = 0;
	for (int y = 0; y < H; y++) {
		for (int x = 0; x < W; x++) {
			png.png_set_pixel(p, x, y, 255 * n / (W * H));
			n++;
		}
	}

    png.png_save(p, "test_gray_stream.png");
    png.png_destroy(p);
}
