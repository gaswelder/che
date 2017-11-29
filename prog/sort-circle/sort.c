import "opt"
import "./ppm.c"

#define S     800           // video size
#define N     360           // number of dots
#define WAIT  1             // pause in seconds between sorts
#define HZ    44100         // audio sample rate
#define FPS   60            // output framerate
#define MINHZ 20            // lowest tone
#define MAXHZ 1000          // highest tone
#define PI 3.141592653589793f

int array[N];
int swaps[N];
const char *message;
FILE *wav;

uint32_t
pcg32(uint64_t *s)
{
    uint64_t m = 0x9b60933458e17d7d;
    uint64_t a = 0xd737232eeccdf7ed;
    *s = *s * m + a;
    int shift = 29 - (*s >> 61);
    return *s >> shift;
}

void
emit_u32le(unsigned long v, FILE *f)
{
    fputc((v >>  0) & 0xff, f);
    fputc((v >>  8) & 0xff, f);
    fputc((v >> 16) & 0xff, f);
    fputc((v >> 24) & 0xff, f);
}

void
emit_u32be(unsigned long v, FILE *f)
{
    fputc((v >> 24) & 0xff, f);
    fputc((v >> 16) & 0xff, f);
    fputc((v >>  8) & 0xff, f);
    fputc((v >>  0) & 0xff, f);
}

void
emit_u16le(unsigned v, FILE *f)
{
    fputc((v >> 0) & 0xff, f);
    fputc((v >> 8) & 0xff, f);
}

unsigned long
hue(int v)
{
    unsigned long h = v / (N / 6);
    unsigned long f = v % (N / 6);
    unsigned long t = 0xff * f / (N / 6);
    unsigned long q = 0xff - t;
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

void
frame(void)
{
    unsigned char buf[S * S * 3];
    memset(buf, 0, sizeof(buf));
    for (int i = 0; i < N; i++) {
        float delta = abs(i - array[i]) / (N / 2.0f);
        float x = -sinf(i * 2.0f * PI / N);
        float y = -cosf(i * 2.0f * PI / N);
        float r = S * 15.0f / 32.0f * (1.0f - delta);
        float px = r * x + S / 2.0f;
        float py = r * y + S / 2.0f;
        ppm_dot(buf, px, py, hue(array[i]));
    }
    if (message) {
        ppm_string(buf, message);
    }
        
    ppm_write(buf, stdout);

    /* Output audio */
    if (wav) {
        int nsamples = HZ / FPS;
        float samples[HZ / FPS];
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

void
swap(int a[N], int i, int j)
{
    int tmp = a[i];
    a[i] = a[j];
    a[j] = tmp;
    swaps[(a - array) + i]++;
    swaps[(a - array) + j]++;
}

void
sort_bubble(int array[N])
{
    int c;
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
    int c;
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
    int c, total = 1;
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

#define SHUFFLE_DRAW  (1u << 0)
#define SHUFFLE_FAST  (1u << 1)

void
shuffle(int array[N], uint64_t *rng, unsigned flags)
{
    message = "Fisher-Yates";
    for (int i = N - 1; i > 0; i--) {
        uint32_t r = pcg32(rng) % (i + 1);
        swap(array, i, r);
        if (flags & SHUFFLE_DRAW) {
            if (!(flags & SHUFFLE_FAST) || i % 2)
            frame();
        }
    }
}

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
    [SORT_RADIX_8_LSD] = "Radix LSD (base 8)",
};

void
run_sort(int type)
{
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

FILE *
wav_init(const char *file)
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

void
usage(const char *name, FILE *f)
{
    fprintf(f, "usage: %s [-a file] [-h] [-q] [s N] [-w N] [-x HEX] [-y]\n",
            name);
    fprintf(f, "  -a       name of audio output (WAV)\n");
    fprintf(f, "  -h       print this message\n");
    fprintf(f, "  -q       don't draw the shuffle\n");
    fprintf(f, "  -s N     animate sort number N (see below)\n");
    fprintf(f, "  -w N     insert a delay of N frames\n");
    fprintf(f, "  -x HEX   use HEX as a 64-bit seed for shuffling\n");
    fprintf(f, "  -y       slow down shuffle animation\n");
    fprintf(f, "\n");
    for (int i = 1; i < SORTS_TOTAL; i++) {
        fprintf(f, "  %d: %s\n", i, sort_names[i]);
    }
}

int
main(int argc, char **argv)
{
    bool help = false;
    bool hide_shuffle = false;
    bool slow_shuffle = false;
    const char *audio_output = NULL;
    int sort_number = 0;
    int delay = 0;
    const char *seed_str = NULL;

    opt(OPT_BOOL, "h", "print the help message", &help);
    opt(OPT_BOOL, "q", "don't draw the shuffle", &hide_shuffle);
    opt(OPT_BOOL, "y", "slow down shuffle animation", &slow_shuffle);
    opt(OPT_STR, "a", "name of audio output (WAV)", &audio_output);
    opt(OPT_INT, "s", "animate sort number N", &sort_number);
    opt(OPT_INT, "w", "insert a delay of N frames", &delay);
    opt(OPT_STR, "x", "seed for shuffling (64-bit HEX string)", &seed_str);
    opt_parse(argc, argv);

    int sorts = 0;
    unsigned flags = SHUFFLE_DRAW | SHUFFLE_FAST;
    uint64_t seed = 0;

    if (help) {
        usage(argv[0], stdout);
        exit(EXIT_SUCCESS);
    }
    if (hide_shuffle) {
        flags &= ~SHUFFLE_DRAW;
    }
    if (slow_shuffle) {
        flags &= ~SHUFFLE_FAST;
    }
    if (audio_output) {
        wav = wav_init(audio_output);
        if (!wav) {
            fprintf(stderr, "%s: %s: %s\n", argv[0], strerror(errno), audio_output);
            exit(EXIT_FAILURE);
        }
    }
    if (seed_str) {
        seed = strtoull(seed_str, 0, 16);
    }
    if (delay) {
        for (int i = 0; i < delay; i++) {
            frame();
        }
    }
    if (sort_number) {
        sorts++;
        frame();
        shuffle(array, &seed, flags);
        run_sort(sort_number);
    }


    for (int i = 0; i < N; i++) {
        array[i] = i;
    }

    /* If no sorts selected, run all of them in order */
    if (!sorts) {
        for (int i = 1; i < SORTS_TOTAL; i++) {
            shuffle(array, &seed, flags);
            run_sort(i);
            for (int i = 0; i < WAIT * FPS; i++) {
                frame();
            }
        }
    }
}
