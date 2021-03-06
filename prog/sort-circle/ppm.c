// https://en.wikipedia.org/wiki/Netpbm_format

#define S     800           // video size
#define R0    (S / 400.0f)  // dot inner radius
#define R1    (S / 200.0f)  // dot outer radius
#define FONT_W 16
#define FONT_H 33
#define PAD   (S / 128)     // message padding


pub void ppm_dot(uint8_t *buf, float x, y, uint32_t fgc)
{
    float fr = 0;
    float fg = 0;
    float fb = 0;
    rgb_split(fgc, &fr, &fg, &fb);

    int miny = floorf(y - R1 - 1);
    int maxy = ceilf(y + R1 + 1);
    int minx = floorf(x - R1 - 1);
    int maxx = ceilf(x + R1 + 1);

    for (int py = miny; py <= maxy; py++) {
        float dy = py - y;
        for (int px = minx; px <= maxx; px++) {
            float dx = px - x;
            float d = sqrtf(dy * dy + dx * dx);
            float a = smoothstep(R1, R0, d);

            uint32_t bgc = ppm_get(buf, px, py);
            float br = 0;
            float bg = 0;
            float bb = 0;
            rgb_split(bgc, &br, &bg, &bb);

            float r = a * fr + (1 - a) * br;
            float g = a * fg + (1 - a) * bg;
            float b = a * fb + (1 - a) * bb;
            ppm_set(buf, px, py, rgb_join(r, g, b));
        }
    }
}

pub void ppm_char(uint8_t *buf, int c, int x, int y, uint32_t fgc)
{
    float fr = 0;
    float fg = 0;
    float fb = 0;
    rgb_split(fgc, &fr, &fg, &fb);
    for (int dy = 0; dy < FONT_H; dy++) {
        for (int dx = 0; dx < FONT_W; dx++) {
            float a = font_value(c, dx, dy);
            if (a > 0.0f) {
                uint32_t bgc = ppm_get(buf, x + dx, y + dy);
                float br = 0;
                float bg = 0;
                float bb = 0;
                rgb_split(bgc, &br, &bg, &bb);

                float r = a * fr + (a - 1) * br;
                float g = a * fg + (a - 1) * bg;
                float b = a * fb + (a - 1) * bb;
                ppm_set(buf, x + dx, y + dy, rgb_join(r, g, b));
            }
        }
    }
}

pub void ppm_string(uint8_t *buf, const char *message)
{
    for (int c = 0; message[c]; c++) {
        ppm_char(buf, message[c], c * FONT_W + PAD, PAD, 0xffffffUL);
    }
}

pub void ppm_write(const uint8_t *buf, FILE *f)
{
    fprintf(f, "P6\n%d %d\n255\n", S, S);
    fwrite(buf, S * 3, S, f);
    fflush(f);
}

void
ppm_set(uint8_t *buf, int x, int y, uint32_t color)
{
    buf[y * S * 3 + x * 3 + 0] = color >> 16;
    buf[y * S * 3 + x * 3 + 1] = color >>  8;
    buf[y * S * 3 + x * 3 + 2] = color >>  0;
}

uint32_t
ppm_get(uint8_t *buf, int x, int y)
{
    uint32_t r = buf[y * S * 3 + x * 3 + 0];
    uint32_t g = buf[y * S * 3 + x * 3 + 1];
    uint32_t b = buf[y * S * 3 + x * 3 + 2];
    return (r << 16) | (g << 8) | b;
}

/* Convert 24-bit color to RGB. */
void
rgb_split(uint32_t c, float *r, float *g, float *b)
{
    *r = ((c >> 16) / 255.0f);
    *g = (((c >> 8) & 0xff) / 255.0f);
    *b = ((c & 0xff) / 255.0f);
}

/* Convert RGB to 24-bit color. */
uint32_t
rgb_join(float r, float g, float b)
{
    uint32_t ir = roundf(r * 255.0f);
    uint32_t ig = roundf(g * 255.0f);
    uint32_t ib = roundf(b * 255.0f);
    return (ir << 16) | (ig << 8) | ib;
}

float clamp(float x, float lower, float upper)
{
    if (x < lower)
        return lower;
    if (x > upper)
        return upper;
    return x;
}

float smoothstep(float lower, float upper, float x)
{
    x = clamp((x - lower) / (upper - lower), 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}
