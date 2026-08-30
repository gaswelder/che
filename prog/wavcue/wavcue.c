#import formats/wav
#import sound
#import opt
#import time

typedef {
    bool loud;
	int64_t duration; // ns
} range_t;

wav.reader_t *r = NULL;
bool loaded = false;
double position = 0;
range_t _val = {};

float SILENCE_LEVEL = 37; // db
float SILENCE_LENGTH = 1; // s
bool cumulative = false;

int main(int argc, char *argv[]) {
	OS.setvbuf(stdout, NULL, OS._IOLBF, 0);
	opt.summary("finds track positions using silence");
	opt.nargs(1, "<wav-file>");
	opt.opt_float("d", "silence level in dB (positive)", &SILENCE_LEVEL);
	opt.opt_float("s", "silencee length in seconds", &SILENCE_LENGTH);
	opt.flag("c", "print cut positions instead of track lengths", &cumulative);
	char **args = opt.parse(argc, argv);

	r = wav.open_reader(args[0]);
    if (r == NULL) {
        panic("failed to open wav");
    }

	int count = 0;
	double last_position = 0;
	while (rmore()) {
		double s = gotosilence();
		if (s < SILENCE_LENGTH) {
			continue;
		}
		double dur = position - last_position;
		last_position = position;
		if (dur < 2) {
			continue;
		}
		count++;
		printf("%02d. track %02d\t", count, count);
		if (cumulative) {
			printtime(position);
		} else {
			printtime(dur);
		}
		putchar('\n');		
	}
	if (wav.more(r)) {
		panic("more");
	}
    wav.close_reader(r);
    return 0;
}

double gotosilence() {
	double sil = 0;
	while (rmore()) {
		// Skip non-silence.
		while (rmore() && peekval() == true) {
			consume();
		}
		// Collect silence.
		sil = 0;
		while (rmore() && peekval() == false) {
			sil += consume();
		}
		// If the silence is too short, it's not silence.
		if (sil > 0.2) {
			break;
		}
	}
	// printf("#\tsilence %.1fs at %6.2f\n", sil, position);
	return sil;
}

bool rmore() {
	return loaded || wav.more(r);
}

bool peekval() {
	if (!loaded) {
		loaded = true;
		_val = readval();
	}
	return _val.loud;
}

double consume() {
	range_t r = _val;
	loaded = false;
	double x = ((double) r.duration) / 1000/1000/1000;
	position += x;
	return x;
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
	char buf[100];
	time.duration_t d = time.newdur((int64_t) (sec * 1000), time.MS);
	time.dur_fmt(&d, buf, 100, "[h]:mm:ss.mmm");
	printf("%s", buf);
}
