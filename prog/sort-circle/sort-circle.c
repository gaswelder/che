/*
 * This is a port of the [https://github.com/skeeto/sort-circle]("sort-circle demo by skeeto).
 */

#import bitwriter
#import formats/ppm
#import opt
#import rnd
#import font.c
#import image
#import lib.c

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

#define FPS   60            // output framerate

#define HZ    44100         // audio sample rate
#define MINHZ 20            // lowest tone
#define MAXHZ 1000          // highest tone
#define PI 3.141592653589793f
int swaps[N] = {};
const char *global_message = "";
const int MESSAGE_PADDING = 800 / 128;
FILE *wav = NULL;

font.t f = {};

int main(int argc, char **argv) {
	f = font.load("prog/sort-circle/sort-circle-font.bin");
	if (!f.data) f = font.load("sort-circle-font.bin");
	if (!f.data) {
		fprintf(stderr, "couldn't load font at '%s'\n", "sort-circle-font.bin");
        return 1;
	}

    bool help = false;
	bool hide_shuffle = false;
	bool slow_shuffle = false;
	char *audio_output = NULL;
	int sort_number = 0;
	int delay = 0;
	char *seed_str = NULL;

	opt.nargs(0, "");
	opt.opt_summary("animated sorting demo - pipe the output to mpv");
    opt.flag("h", "print the help message", &help);
    opt.flag("q", "don't draw the shuffle", &hide_shuffle);
    opt.flag("y", "slow down shuffle animation", &slow_shuffle);
    opt.str("a", "name of audio output (WAV)", &audio_output);
    opt.opt_int("s", "animate sort number N", &sort_number);
    opt.opt_int("w", "insert a delay of N frames", &delay);
    opt.str("x", "seed for shuffling (64-bit HEX string)", &seed_str);
    opt.parse(argc, argv);

    if (help) {
        opt.usage();
        for (int i = 1; i < SORTS_TOTAL; i++) {
            fprintf(stderr, "  %d: %s\n", i, sort_names[i]);
        }
        exit(0);
    }

    if (seed_str) {
        rnd.seed(strtoull(seed_str, NULL, 16));
    }

    if (audio_output) {
		wav = fopen(audio_output, "wb");
		if (!wav) {
            fprintf(stderr, "%s: %s: %s\n", argv[0], strerror(errno), audio_output);
            exit(0);
        }
        wav_init(wav);
    }

	image.image_t *img = image.new(800, 800);

    if (delay) {
        for (int i = 0; i < delay; i++) {
            frame(img);
        }
    }

    /*
     * Fill the array
     */
    for (int i = 0; i < N; i++) {
        array[i] = i;
    }

    for (int i = 1; i < SORTS_TOTAL; i++) {
        shuffle(img, array, hide_shuffle, slow_shuffle);
        run_sort(img, i);
        pause_1s(img);
    }
    return 0;
}

void shuffle(image.image_t *img, int array[N], bool hide, slow) {
    global_message = "Fisher-Yates";
    for (int i = N - 1; i > 0; i--) {
        uint32_t r = rnd.u32() % (i + 1);
        swap(array, i, r);
        if (hide) {
            continue;
        }
        if (slow || i % 2) {
            frame(img);
        }
    }
}

void run_sort(image.image_t *img, int type) {
    if (type > 0 && type < SORTS_TOTAL) {
        global_message = sort_names[type];
    } else {
        global_message = 0;
    }
    switch (type) {
        case SORT_NULL: {}
        case SORT_ODD_EVEN: { sort_odd_even(img, array); }
        case SORT_BUBBLE: { sort_bubble(img, array); }
        case SORT_INSERTION: { sort_insertion(img, array); }
        case SORT_STOOGESORT: { sort_stoogesort(img, array, 0, N - 1); }
        case SORT_QUICKSORT: { sort_quicksort(img, array, N); }
        case SORT_RADIX_8_LSD: { sort_radix_lsd(img, array, 8); }
        case SORTS_TOTAL: {}
    }
    frame(img);
}

void pause_1s(image.image_t *img) {
	for (int i = 0; i < FPS; i++) frame(img);
}

void frame(image.image_t *img) {
    draw_array(img);
    if (global_message) {
        draw_string(img, f, global_message);
    }
    ppm.writeimg(img, stdout);
	fflush(stdout);
	image.clear(img);

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
		bitwriter.t w = { wav };
        for (int i = 0; i < nsamples; i++) {
            int s = samples[i] * 0x7fff;
            bitwriter.le16(&w, s);
        }
        fflush(wav);
    }

    memset(swaps, 0, sizeof(swaps));
}

void draw_array(image.image_t *img) {
	float S = 800;
	for (int i = 0; i < N; i++) {
        float delta = fabs((float)(i - array[i])) / (N / 2.0f);
        float x = -sinf(i * 2.0f * PI / N);
        float y = -cosf(i * 2.0f * PI / N);
        float r = S * 15.0f / 32.0f * (1.0f - delta);
        float px = r * x + S / 2.0f;
        float py = r * y + S / 2.0f;

        int32_t h = hue(array[i]);
        image.rgb_t color = {
            .red = ((h >> 16)),
            .green = (((h >> 8) & 0xff)),
            .blue = ((h & 0xff))
        };
        draw_dot(img, px, py, color);
    }
}

void swap(int a[N], int i, int j)
{
    int tmp = a[i];
    a[i] = a[j];
    a[j] = tmp;
    swaps[(a - array) + i]++;
    swaps[(a - array) + j]++;
}

void sort_bubble(image.image_t *img, int array[N]) {
    int c = 0;
    while (1) {
        c = 0;
        for (int i = 1; i < N; i++) {
            if (array[i - 1] > array[i]) {
                swap(array, i - 1, i);
                c = 1;
            }
        }
        frame(img);
        if (!c) break;
    }
}

void sort_odd_even(image.image_t *img, int array[N]) {
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
        frame(img);
        if (!c) break;
    }
}

void sort_insertion(image.image_t *img, int array[N])
{
    for (int i = 1; i < N; i++) {
        for (int j = i; j > 0 && array[j - 1] > array[j]; j--) {
            swap(array, j, j - 1);
        }
        frame(img);
    }
}

void sort_stoogesort(image.image_t *img, int *array, int i, int j) {
    int c = 0;
    if (array[i] > array[j]) {
        swap(array, i, j);
        if (c++ % 32 == 0)
            frame(img);
    }
    if (j - i + 1 > 2) {
        int t = (j - i + 1) / 3;
        sort_stoogesort(img, array, i, j - t);
        sort_stoogesort(img, array, i + t, j);
        sort_stoogesort(img, array, i, j - t);
    }
}

void sort_quicksort(image.image_t *img, int *array, int n) {
    if (n > 1) {
        int high = n;
        int i = 1;
        while (i < high) {
            if (array[0] < array[i]) {
                swap(array, i, --high);
                if (n > 12)
                    frame(img);
            } else {
                i++;
            }
        }
        swap(array, 0, --high);
        frame(img);
        sort_quicksort(img, array, high + 1);
        sort_quicksort(img, array + high + 1, n - high - 1);
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

void sort_radix_lsd(image.image_t *img, int array[N], int b) {
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
            frame(img);
            if (!c) break;
        }
    }
}

void wav_init(FILE *f) {
	bitwriter.t w = { f };

    bitwriter.be32(&w, 0x52494646UL); // "RIFF"
	bitwriter.le32(&w, 0xffffffffUL); // file length
	bitwriter.be32(&w, 0x57415645UL); // "WAVE"
	bitwriter.be32(&w, 0x666d7420UL); // "fmt "
	bitwriter.le32(&w, 16); // struct size
	bitwriter.le16(&w, 1); // PCM
	bitwriter.le16(&w, 1); // mono
	bitwriter.le32(&w, HZ); // sample rate (i.e. 44.1 kHz)
	bitwriter.le32(&w, HZ * 2); // byte rate
	bitwriter.le16(&w, 2); // block size
	bitwriter.le16(&w, 16); // bits per sample
	bitwriter.be32(&w, 0x64617461UL); // "data"
	bitwriter.le32(&w, 0xffffffffUL); // byte length
}



void draw_dot(image.image_t *img, float x, y, image.rgb_t color) {
	float S = 800; // image size
	float R0 = S  / 400.0f;  // dot inner radius
	float R1 = S / 200.0f;  // dot outer radius

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
			image.blend(img, px, py, color, alpha);
        }
    }
}

float smoothstep(float lower, float upper, float x) {
    x = lib.clampf((x - lower) / (upper - lower), 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

void draw_string(image.image_t *img, font.t f, const char *message) {
    image.rgb_t fontcolor = {255, 255, 255};
    for (int c = 0; message[c]; c++) {
        int x = c * f.w + MESSAGE_PADDING;
        int y = MESSAGE_PADDING;
        for (int dy = 0; dy < f.h; dy++) {
            for (int dx = 0; dx < f.w; dx++) {
                float alpha = font.value(f, message[c], dx, dy);
                if (alpha > 0.0f) {
                	image.blend(img, x + dx, y + dy, fontcolor, alpha);
                }
            }
        }
    }
}

uint32_t hue(int v) {
    uint32_t h = v / (N / 6);
    uint32_t f = v % (N / 6);
    uint32_t t = 0xff * f / (N / 6);
    uint32_t q = 0xff - t;
    switch (h) {
        case 0: { return 0xff0000UL | (t << 8); }
        case 1: { return (q << 16) | 0x00ff00UL; }
        case 2: { return 0x00ff00UL | t; }
        case 3: { return (q << 8) | 0x0000ffUL; }
        case 4: { return (t << 16) | 0x0000ffUL; }
        case 5: { return 0xff0000UL | q; }
        default: { panic("!"); }
    }
}