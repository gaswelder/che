#import png

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
    libattopng_t *png = libattopng_new(W, H, PNG_PALETTE);
    uint32_t palette[] = {
            RGBA(0, 0, 0xff, 0xff),
            RGBA(0, 0xff, 0, 0x80),
            RGBA(0xff, 0, 0, 0xff),
            RGBA(0xff, 0, 0xff, 0x80)
    };
    libattopng_set_palette(png, palette, 4);

    int x, y;
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            libattopng_set_pixel(png, x, y, (x % 16) / 4);
        }
    }

    libattopng_save(png, "test_palette.png");
    libattopng_destroy(png);

    // -----------------

    png = libattopng_new(W, H, PNG_RGBA);

    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            libattopng_set_pixel(png, x, y, RGBA(x & 255, y & 255, 128, (255 - ((x / 2) & 255))));
        }
    }
    libattopng_save(png, "test_rgba.png");
    libattopng_destroy(png);

    // -----------------

    png = libattopng_new(W, H, PNG_RGB);

    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            libattopng_set_pixel(png, x, y, RGB(x & 255, y & 255, 128));
        }
    }
    libattopng_save(png, "test_rgb.png");
    libattopng_destroy(png);

    // -----------------

    png = libattopng_new(W, H, PNG_GRAYSCALE);

    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            libattopng_set_pixel(png, x, y, (((x + y) / 2) & 255));
        }
    }
    libattopng_save(png, "test_gray.png");
    libattopng_destroy(png);

    // -----------------

    png = libattopng_new(W, H, PNG_GRAYSCALE_ALPHA);

    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            libattopng_set_pixel(png, x, y, ALPHA(((x + y) / 2) & 255, 255 - ((y / 2) & 255)));
        }
    }
    libattopng_save(png, "test_gray_alpha.png");
    libattopng_destroy(png);

    // -----------------

    png = libattopng_new(W, H, PNG_GRAYSCALE);

    libattopng_start_stream(png, 0, 0);
    for (x = 0; x < W * H; x++) {
        libattopng_put_pixel(png, 255 * x / (W * H));
    }
    libattopng_save(png, "test_gray_stream.png");
    libattopng_destroy(png);

    return 0;
}
