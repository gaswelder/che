import "fileutil"

#define FONT_W 16
#define FONT_H 33

uint8_t *font = NULL;

bool load_font(const char *path) {
    font = (uint8_t *) readfile(path, NULL);
    return font != NULL;
}

pub float font_value(int c, int x, int y) {
    if (c < 32 || c > 127)
        return 0.0f;
    int cx = c % 16;
    int cy = (c - 32) / 16;
    int v = font[(cy * FONT_H + y) * FONT_W * 16 + (cx * FONT_W) + x];
    return sqrtf(v / 255.0f);
}