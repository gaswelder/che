#import formats/srt
#import time

int main(int argc, char *argv[]) {
	if (argc != 5) {
		fprintf(stderr, "arguments are srt timestamps: <have1> <want1> <have2> <want2>\n");
		return 1;
	}
	int64_t have1 = srt.parsepos(argv[1]).us;
	int64_t want1 = srt.parsepos(argv[2]).us;

	int64_t have2 = srt.parsepos(argv[3]).us;
	int64_t want2 = srt.parsepos(argv[4]).us;

	double k = (double)(want2 - want1) / (double)(have2 - have1);
	double s = (double) want1 - (double) have1 * k;

	srt.reader_t r = {};
	srt.block_t b = {};
	while (srt.readblock(&r, &b)) {
		adjust(&b.begin, k, s);
		adjust(&b.end, k, s);
		srt.printblock(&b);
	}
	return 0;
}

void adjust(time.duration_t *d, double k, b) {
	double val = (double) d->us;
	val = val * k + b;
	d->us = (int64_t) val;
}
