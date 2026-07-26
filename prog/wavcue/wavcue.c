#import formats/wav
#import sound
#import opt

typedef {
    bool loud;
    double duration; // seconds
} range_t;

const int RESOLUTION = 10;
float SILENCE_LEVEL = 37; // db

int main(int argc, char *argv[]) {
	opt.summary("detects track split points by silence");
	opt.nargs(1, "<wav-file>");
	opt.opt_float("l", "silence level in dB (positive)", &SILENCE_LEVEL);
	char **args = opt.parse(argc, argv);
    wav.reader_t *r = wav.open_reader(args[0]);
    if (r == NULL) {
        panic("failed to open wav");
    }

    range_t ranges[100] = {};
    int i = 0;
    while (wav.more(r)) {
        bool x = window(r, 44100 / RESOLUTION);
        if (x != ranges[i].loud) {
            i++;
            if (i == 100) {
                panic("> 100");
            }
            ranges[i].loud = x;
        }
        ranges[i].duration += 1.0 / RESOLUTION;
    }
    wav.close_reader(r);

    printlist(ranges, i+1);

    return 0;
}

bool window(wav.reader_t *r, int size) {
    if (!wav.more(r)) {
        return 0;
    }
    double sum = 0;
    int i = 0;
    for (; i < size; i++) {
        if (!wav.more(r)) {
            break;
        }
        sound.samplef_t s = wav.read_samplef(r);
        double x = (s.left * s.left + s.right * s.right) / 2.0;
        sum += x;
    }

    double rms = sqrt(sum / i);
    if (rms < 1e-12) rms = 1e-12;
    double db = 20.0 * log10(rms);

    return db >= -SILENCE_LEVEL;
}

void printlist(range_t *ranges, int n) {
    // We're guaranteed to have 0-th entry as quiet.
    double tracklen = ranges[0].duration;
    int num = 1;

    for (int i = 1; i < n; i += 2) {
        tracklen += ranges[i].duration + ranges[i+1].duration;
        printf("%02d. track %02d\t", num, num);
        printtime(tracklen);
        putchar('\n');
        tracklen = 0;
        num++;
    }
}

void printtime(double sec) {
    // printf(" (%fs) ", sec);
    int x = (int) (sec * 1000);

    // ms
    int ms = x % 1000;
    x /= 1000;

    // s
    int s = x % 60;
    x /= 60;
    int m = x;

    printf("%d:%02d.%03d", m, s, ms);
}
