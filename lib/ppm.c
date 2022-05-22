// https://en.wikipedia.org/wiki/Netpbm_format

// Pipe the output into mpv

typedef { float r, g, b; } rgb_t;

typedef {
    uint8_t *buf;
    rgb_t *frame;
    int S;
} ppm_t;

pub ppm_t *ppm_init(int size) {
    ppm_t *p = calloc(1, sizeof(ppm_t));
    p->S = size;
    p->frame = calloc(size * size, sizeof(rgb_t));
    return p;
}

/**
 * Sets pixel color at given frame coordinates.
 */
pub void ppm_set(ppm_t *p, int x, y, rgb_t color)
{
    p->frame[x + y * p->S] = color;
}

/**
 * Returns pixel color at given frame coordinates.
*/
pub rgb_t ppm_get(ppm_t *p, int x, y)
{
    return p->frame[x + p->S * y];
}

pub void ppm_merge(ppm_t *p, int x, y, rgb_t color, float a)
{
    rgb_t newcolor = ppm_get(p, x, y);
    newcolor.r = a * color.r + (a - 1) * newcolor.r;
    newcolor.g = a * color.g + (a - 1) * newcolor.g;
    newcolor.b = a * color.b + (a - 1) * newcolor.b;
    ppm_set(p, x, y, newcolor);
}

/**
 * Writes the current frame buffer to the given file and clears the frame
 * buffer.
 */
pub void ppm_write(ppm_t *p, FILE *f)
{
    fprintf(f, "P6\n%d %d\n255\n", p->S, p->S);
    
    uint32_t buf = 0;
    for (int i = 0; i < p->S * p->S; i++) {
        buf = format_rgb(p->frame[i]);
        fwrite(&buf, 3, 1, f);
    }
    fflush(f);
    memset(p->frame, 0, p->S * p->S * sizeof(rgb_t));
}

/* Convert RGB to 24-bit color. */
uint32_t format_rgb(rgb_t c)
{
    uint32_t ir = roundf(c.r * 255.0f);
    uint32_t ig = roundf(c.g * 255.0f);
    uint32_t ib = roundf(c.b * 255.0f);
    return (ir << 16) | (ig << 8) | ib;
}
