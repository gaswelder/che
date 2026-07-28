#import formats/wav
#import sound
#import opt

typedef {
    bool loud;
    double duration; // seconds
} range_t;

const int RESOLUTION = 10;
float SILENCE_LEVEL = 37; // db

wav.reader_t *r = NULL;
bool loaded = false;
range_t _val = {};

int main(int argc, char *argv[]) {
	opt.summary("detects track split points by silence");
	opt.nargs(1, "<wav-file>");
	opt.opt_float("l", "silence level in dB (positive)", &SILENCE_LEVEL);
	char **args = opt.parse(argc, argv);

	r = wav.open_reader(args[0]);
    if (r == NULL) {
        panic("failed to open wav");
    }

	emit(run(false) + run(true) + run(false));
	while (wav.more(r)) {
		emit(run(true) + run(false));
	}
    wav.close_reader(r);
    return 0;
}

int count = 0;
void emit(double dur) {
	count++;
	printf("%02d. track %02d\t", count, count);
	printtime(dur);
	printf("\n");
}

double run(bool x) {
	double dur = 0;
	while ((loaded || wav.more(r)) && next(x)) {
		dur += consume();
	}
	return dur;
}

bool next(bool x) {
	if (!loaded) {
		loaded = true;
		_val = readval();
	}
	return _val.loud == x;
}

double consume() {
	range_t r = _val;
	loaded = false;
	return r.duration;
}

range_t readval() {
	if (!wav.more(r)) {
		panic("nomore");
	}
	double sum = 0;
    int i = 0;
	int size = 44100 / RESOLUTION;
    for (; i < size; i++) {
        if (!wav.more(r)) {
            break;
        }
        sound.samplef_t s = wav.read_samplef(r);
        sum += (s.left * s.left + s.right * s.right) / 2.0;
    }
    double rms = max(sqrt(sum / i), 1e-12);
    double db = 20.0 * log10(rms);
	range_t res = {};
	res.loud = db >= -SILENCE_LEVEL;
	res.duration = 1.0 / RESOLUTION;
	return res;
}

double max(double x, y) {
	if (x > y) return x;
	return y;
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
