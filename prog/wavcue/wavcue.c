#import formats/wav
#import sound
#import opt

typedef {
    bool loud;
	int64_t duration; // ns
} range_t;

float SILENCE_LEVEL = 37; // db

wav.reader_t *r = NULL;
bool loaded = false;
range_t _val = {};

bool cumulative = false;

int main(int argc, char *argv[]) {
	OS.setvbuf(stdout, NULL, OS._IOLBF, 0);
	opt.summary("detects track split points by silence");
	opt.nargs(1, "<wav-file>");
	opt.opt_float("l", "silence level in dB (positive)", &SILENCE_LEVEL);
	opt.flag("c", "print cut positions instead of track lengths", &cumulative);
	char **args = opt.parse(argc, argv);

	r = wav.open_reader(args[0]);
    if (r == NULL) {
        panic("failed to open wav");
    }

	double dur = 0;
	while ((loaded || wav.more(r)) && next(false)) {
		dur += consume();
	}
	emit(dur + track());
	while (wav.more(r)) {
		emit(track());
	}
	// printf("total = ");
	// printtime(total);
	// putchar('\n');
    wav.close_reader(r);
    return 0;
}

double track() {
	double dur = 0;
	while (rmore()) {
		// Non-silence.
		while (rmore() && next(true)) {
			dur += consume();
		}

		// Silence.
		double sil = 0;
		while (rmore() && next(false)) {
			sil += consume();
		}
		dur += sil;
		if (sil > 1) {
			break;
		}
	}
	return dur;
}

bool rmore() {
	return loaded || wav.more(r);
}

int count = 0;
double total = 0;
void emit(double dur) {
	count++;
	total += dur;
	printf("%02d. track %02d\t", count, count);
	if (cumulative) {
		printtime(total);
	} else {
		printtime(dur);
	}
	putchar('\n');
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
	return ((double) r.duration) / 1000/1000/1000;
}

range_t readval() {
	if (!wav.more(r)) {
		panic("nomore");
	}
    sound.samplef_t s = wav.read_samplef(r);
    double e = (s.left * s.left + s.right * s.right) / 2.0;
    double rms = max(sqrt(e / 1), 1e-12);
    double db = 20.0 * log10(rms);
	range_t res = {};
	res.loud = db >= -SILENCE_LEVEL;
	res.duration = 1000 * 1000 * 1000 / 44100;
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

	// m
    int m = x % 60;
	x /= 60;

	// h
	int h = x;

	if (h > 0) {
		printf("%d:%02d:%02d.%03d", h, m, s, ms);
	} else {
		printf("%d:%02d.%03d", m, s, ms);
	}
}
