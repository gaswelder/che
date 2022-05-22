/*
 * This is a port of the [https://github.com/skeeto/sort-circle]("sort-circle demo by skeeto).
 */

import "opt"
import "cli"
import "ppm"
import "prng/pcg32"
import "fileutil"

/*
 * The array that's being shuffled and sorted.
 */
#define N     360
int array[N] = {};

/*
 * The sorting algorithms.
 */
enum {
    SORT_NULL,
    SORT_BUBBLE,
    SORT_ODD_EVEN,
    SORT_INSERTION,
    SORT_STOOGESORT,
    SORT_QUICKSORT,
    SORT_RADIX_8_LSD,

    SORTS_TOTAL
};
const char *sort_names[] = {
    [SORT_ODD_EVEN] = "Odd-even",
    [SORT_BUBBLE] = "Bubble",
    [SORT_INSERTION] = "Insertion",
    [SORT_STOOGESORT] = "Stoogesort",
    [SORT_QUICKSORT] = "Quicksort",
    [SORT_RADIX_8_LSD] = "Radix LSD (base 8)"
};

/**
 * PPM buffer
 */
const int S = 800;
ppm_t *ppm = NULL;

#define FPS   60            // output framerate

#define HZ    44100         // audio sample rate
#define MINHZ 20            // lowest tone
#define MAXHZ 1000          // highest tone
#define PI 3.141592653589793f
int swaps[N] = {};
const char *message = "";
FILE *wav = NULL;

int main(int argc, char **argv)
{
    if (!load_font("prog/sort-circle-font.bin")) {
        fatal("couldn't load font at 'prog/sort-circle-font.bin'");
    }

    ppm = ppm_init(S);

    /*
     * Parse the flags.
     */
    bool help = false;
    opt(OPT_BOOL, "h", "print the help message", &help);
    bool hide_shuffle = false;
    opt(OPT_BOOL, "q", "don't draw the shuffle", &hide_shuffle);
    bool slow_shuffle = false;
    opt(OPT_BOOL, "y", "slow down shuffle animation", &slow_shuffle);
    const char *audio_output = NULL;
    opt(OPT_STR, "a", "name of audio output (WAV)", &audio_output);
    int sort_number = 0;
    opt(OPT_INT, "s", "animate sort number N", &sort_number);
    int delay = 0;
    opt(OPT_INT, "w", "insert a delay of N frames", &delay);
    const char *seed_str = NULL;
    opt(OPT_STR, "x", "seed for shuffling (64-bit HEX string)", &seed_str);
    opt_parse(argc, argv);

    if (help) {
        opt_usage();
        for (int i = 1; i < SORTS_TOTAL; i++) {
            fprintf(stderr, "  %d: %s\n", i, sort_names[i]);
        }
        exit(EXIT_SUCCESS);
    }

    if (seed_str) {
        pcg32_seed(strtoull(seed_str, NULL, 16));
    }

    if (audio_output) {
        wav = wav_init(audio_output);
        if (!wav) {
            fprintf(stderr, "%s: %s: %s\n", argv[0], strerror(errno), audio_output);
            exit(EXIT_FAILURE);
        }
    }

    if (delay) {
        for (int i = 0; i < delay; i++) {
            frame();
        }
    }

    /*
     * Fill the array
     */
    for (int i = 0; i < N; i++) {
        array[i] = i;
    }

    for (int i = 1; i < SORTS_TOTAL; i++) {
        shuffle(array, hide_shuffle, slow_shuffle);
        run_sort(i);
        /*
         * Pause for 1 second
         */
        for (int i = 0; i < FPS; i++) {
            frame();
        }
    }
}

void emit_u32le(uint32_t v, FILE *f)
{
    fputc((v >>  0) & 0xff, f);
    fputc((v >>  8) & 0xff, f);
    fputc((v >> 16) & 0xff, f);
    fputc((v >> 24) & 0xff, f);
}

void emit_u32be(uint32_t v, FILE *f)
{
    fputc((v >> 24) & 0xff, f);
    fputc((v >> 16) & 0xff, f);
    fputc((v >>  8) & 0xff, f);
    fputc((v >>  0) & 0xff, f);
}

void emit_u16le(unsigned v, FILE *f)
{
    fputc((v >> 0) & 0xff, f);
    fputc((v >> 8) & 0xff, f);
}

uint32_t hue(int v)
{
    uint32_t h = v / (N / 6);
    uint32_t f = v % (N / 6);
    uint32_t t = 0xff * f / (N / 6);
    uint32_t q = 0xff - t;
    switch (h) {
        case 0:
            return 0xff0000UL | (t << 8);
        case 1:
            return (q << 16) | 0x00ff00UL;
        case 2:
            return 0x00ff00UL | t;
        case 3:
            return (q << 8) | 0x0000ffUL;
        case 4:
            return (t << 16) | 0x0000ffUL;
        case 5:
            return 0xff0000UL | q;
    }
    abort();
    return -1;
}

void frame() {
    for (int i = 0; i < N; i++) {
        float delta = fabs(i - array[i]) / (N / 2.0f);
        float x = -sinf(i * 2.0f * PI / N);
        float y = -cosf(i * 2.0f * PI / N);
        float r = S * 15.0f / 32.0f * (1.0f - delta);
        float px = r * x + S / 2.0f;
        float py = r * y + S / 2.0f;

        int32_t c = hue(array[i]);
        rgb_t color = {
            .r = ((c >> 16) / 255.0f),
            .g = (((c >> 8) & 0xff) / 255.0f),
            .b = ((c & 0xff) / 255.0f)
        };
        ppm_dot(ppm, px, py, color);
    }
    if (message) {
        draw_string(ppm, message);
    }
    ppm_write(ppm, stdout);

    /* Output audio */
    if (wav) {
        int nsamples = HZ / FPS;
        float samples[HZ / FPS] = {};
        memset(samples, 0, sizeof(samples));

        /* How many voices to mix? */
        int voices = 0;
        for (int i = 0; i < N; i++) {
            voices += swaps[i];
        }

        /* Generate each voice */
        for (int i = 0; i < N; i++) {
            if (swaps[i]) {
                float hz = i * (MAXHZ - MINHZ) / (float)N + MINHZ;
                for (int j = 0; j < nsamples; j++) {
                    float u = 1.0f - j / (float)(nsamples - 1);
                    float parabola = 1.0f - (u * 2 - 1) * (u * 2 - 1);
                    float envelope = parabola * parabola * parabola;
                    float v = sinf(j * 2.0f * PI / HZ * hz) * envelope;
                    samples[j] += swaps[i] * v / voices;
                }
            }
        }

        /* Write out 16-bit samples */
        for (int i = 0; i < nsamples; i++) {
            int s = samples[i] * 0x7fff;
            emit_u16le(s, wav);
        }
        fflush(wav);
    }

    memset(swaps, 0, sizeof(swaps));
}

void swap(int a[N], int i, int j)
{
    int tmp = a[i];
    a[i] = a[j];
    a[j] = tmp;
    swaps[(a - array) + i]++;
    swaps[(a - array) + j]++;
}

void sort_bubble(int array[N])
{
    int c = 0;
    while (1) {
        c = 0;
        for (int i = 1; i < N; i++) {
            if (array[i - 1] > array[i]) {
                swap(array, i - 1, i);
                c = 1;
            }
        }
        frame();
        if (!c) break;
    }
}

void
sort_odd_even(int array[N])
{
    int c = 0;
    while (1) {
        c = 0;
        for(int i = 1; i < N - 1; i += 2) {
            if (array[i] > array[i + 1]) {
                swap(array, i, i + 1);
                c = 1;
            }
        }
        for (int i = 0; i < N - 1; i += 2) {
            if (array[i] > array[i + 1]) {
                swap(array, i, i + 1);
                c = 1;
            }
        }
        frame();
        if (!c) break;
    }
}

void
sort_insertion(int array[N])
{
    for (int i = 1; i < N; i++) {
        for (int j = i; j > 0 && array[j - 1] > array[j]; j--) {
            swap(array, j, j - 1);
        }
        frame();
    }
}

void
sort_stoogesort(int *array, int i, int j)
{
    int c = 0;
    if (array[i] > array[j]) {
        swap(array, i, j);
        if (c++ % 32 == 0)
            frame();
    }
    if (j - i + 1 > 2) {
        int t = (j - i + 1) / 3;
        sort_stoogesort(array, i, j - t);
        sort_stoogesort(array, i + t, j);
        sort_stoogesort(array, i, j - t);
    }
}

void
sort_quicksort(int *array, int n)
{
    if (n > 1) {
        int high = n;
        int i = 1;
        while (i < high) {
            if (array[0] < array[i]) {
                swap(array, i, --high);
                if (n > 12)
                    frame();
            } else {
                i++;
            }
        }
        swap(array, 0, --high);
        frame();
        sort_quicksort(array, high + 1);
        sort_quicksort(array + high + 1, n - high - 1);
    }
}

int
digit(int v, int b, int d)
{
    for (int i = 0; i < d; i++) {
        v /= b;
    }
    return v % b;
}

void
sort_radix_lsd(int array[N], int b)
{
    int c = 0;
    int total = 1;
    for (int d = 0; total; d++) {
        total = -1;
        /* Odd-even sort on the current digit */
        while (1) {
            total++;
            c = 0;
            for(int i = 1; i < N - 1; i += 2) {
                if (digit(array[i], b, d) > digit(array[i + 1], b, d)) {
                    swap(array, i, i + 1);
                    c = 1;
                }
            }
            for (int i = 0; i < N - 1; i += 2) {
                if (digit(array[i], b, d) > digit(array[i + 1], b, d)) {
                    swap(array, i, i + 1);
                    c = 1;
                }
            }
            frame();
            if (!c) break;
        }
    }
}

void shuffle(int array[N], bool hide, slow) {
    message = "Fisher-Yates";
    for (int i = N - 1; i > 0; i--) {
        uint32_t r = pcg32() % (i + 1);
        swap(array, i, r);
        if (hide) {
            continue;
        }
        if (slow || i % 2) {
            frame();
        }
    }
}

void run_sort(int type) {
    if (type > 0 && type < SORTS_TOTAL)
        message = sort_names[type];
    else
        message = 0;
    switch (type) {
        case SORT_NULL:
            break;
        case SORT_ODD_EVEN:
            sort_odd_even(array);
            break;
        case SORT_BUBBLE:
            sort_bubble(array);
            break;
        case SORT_INSERTION:
            sort_insertion(array);
            break;
        case SORT_STOOGESORT:
            sort_stoogesort(array, 0, N - 1);
            break;
        case SORT_QUICKSORT:
            sort_quicksort(array, N);
            break;
        case SORT_RADIX_8_LSD:
            sort_radix_lsd(array, 8);
            break;
        case SORTS_TOTAL:
            break;
    }
    frame();
}

FILE *wav_init(const char *file)
{
    FILE *f = fopen(file, "wb");
    if (f) {
        emit_u32be(0x52494646UL, f); // "RIFF"
        emit_u32le(0xffffffffUL, f); // file length
        emit_u32be(0x57415645UL, f); // "WAVE"
        emit_u32be(0x666d7420UL, f); // "fmt "
        emit_u32le(16,           f); // struct size
        emit_u16le(1,            f); // PCM
        emit_u16le(1,            f); // mono
        emit_u32le(HZ,           f); // sample rate (i.e. 44.1 kHz)
        emit_u32le(HZ * 2,       f); // byte rate
        emit_u16le(2,            f); // block size
        emit_u16le(16,           f); // bits per sample
        emit_u32be(0x64617461UL, f); // "data"
        emit_u32le(0xffffffffUL, f); // byte length
    }
    return f;
}

#define R0    (S / 400.0f)  // dot inner radius
#define R1    (S / 200.0f)  // dot outer radius
void ppm_dot(ppm_t *p, float x, y, rgb_t color)
{
    int miny = floorf(y - R1 - 1);
    int maxy = ceilf(y + R1 + 1);
    int minx = floorf(x - R1 - 1);
    int maxx = ceilf(x + R1 + 1);

    for (int py = miny; py <= maxy; py++) {
        float dy = py - y;
        for (int px = minx; px <= maxx; px++) {
            float dx = px - x;
            float d = sqrtf(dy * dy + dx * dx);
            float alpha = smoothstep(R1, R0, d);
            ppm_merge(p, px, py, color, alpha);
        }
    }
}

float smoothstep(float lower, float upper, float x)
{
    x = clamp((x - lower) / (upper - lower), 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

float clamp(float x, float lower, float upper)
{
    if (x < lower)
        return lower;
    if (x > upper)
        return upper;
    return x;
}


const int PAD = 800 / 128;     // message padding
#define FONT_W 16
#define FONT_H 33
void draw_string(ppm_t *p, const char *message)
{
    rgb_t fontcolor = {1.0, 1.0, 1.0};
    for (int c = 0; message[c]; c++) {
        int x = c * FONT_W + PAD;
        int y = PAD;
        for (int dy = 0; dy < FONT_H; dy++) {
            for (int dx = 0; dx < FONT_W; dx++) {
                float alpha = font_value(message[c], dx, dy);
                if (alpha > 0.0f) {
                    ppm_merge(p, x + dx, y + dy, fontcolor, alpha);
                }
            }
        }
    }
}

uint8_t *font = NULL;

bool load_font(const char *path) {
    font = (uint8_t *) readfile(path, NULL);
    return font != NULL;
}

float font_value(int c, int x, int y) {
    if (c < 32 || c > 127) {
        return 0.0f;
    }
    int cx = c % 16;
    int cy = (c - 32) / 16;
    int v = font[(cy * FONT_H + y) * FONT_W * 16 + (cx * FONT_W) + x];
    return sqrtf(v / 255.0f);
}