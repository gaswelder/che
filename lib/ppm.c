// https://en.wikipedia.org/wiki/Netpbm_format

// Pipe the output into mpv

pub typedef { float r, g, b; } rgb_t;

pub typedef {
    rgb_t *frame;
    int width, height;
} ppm_t;

pub ppm_t *init(int width, height) {
    ppm_t *p = calloc(1, sizeof(ppm_t));
    p->width = width;
    p->height = height;
    p->frame = calloc(width * height, sizeof(rgb_t));
    return p;
}

/**
 * Sets pixel color at given frame coordinates.
 */
pub void set(ppm_t *p, int x, y, rgb_t color)
{
    p->frame[x + y * p->width] = color;
}

/**
 * Returns pixel color at given frame coordinates.
*/
pub rgb_t get(ppm_t *p, int x, y)
{
    return p->frame[x + p->width * y];
}

pub void merge(ppm_t *p, int x, y, rgb_t color, float a)
{
    rgb_t newcolor = get(p, x, y);
    newcolor.r = a * color.r + (a - 1) * newcolor.r;
    newcolor.g = a * color.g + (a - 1) * newcolor.g;
    newcolor.b = a * color.b + (a - 1) * newcolor.b;
    set(p, x, y, newcolor);
}

/**
 * Writes the current frame buffer to the given file and clears the frame
 * buffer.
 */
pub void write(ppm_t *p, FILE *f)
{
    fprintf(f, "P6\n%d %d\n255\n", p->width, p->height);
    int size = p->width * p->height;
    uint32_t buf = 0;
    for (int i = 0; i < size; i++) {
        buf = format_rgb(p->frame[i]);
        fwrite(&buf, 3, 1, f);
    }
}

pub void clear(ppm_t *p)
{
    memset(p->frame, 0, p->width * p->height * sizeof(rgb_t));
}

/* Convert RGB to 24-bit color. */
uint32_t format_rgb(rgb_t c)
{
    uint32_t ir = roundf(c.r * 255.0f);
    uint32_t ig = roundf(c.g * 255.0f);
    uint32_t ib = roundf(c.b * 255.0f);
    return (ir << 16) | (ig << 8) | ib;
}
