#import fs

pub typedef {
	int w, h;
	uint8_t *data;
} t;

pub float value(t f, int c, int x, int y) {
    if (c < 32 || c > 127) {
        return 0.0f;
    }
    int cx = c % 16;
    int cy = (c - 32) / 16;
    int v = f.data[(cy * f.h + y) * f.w * 16 + (cx * f.w) + x];
    return sqrtf(v / 255.0f);
}

pub t load(const char *path) {
	t f = { .w = 16, .h = 33, .data = NULL };
	f.data = fs.readfile(path, NULL);
	return f;
}
