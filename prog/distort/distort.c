#import formats/wav
#import opt

enum {
    HARD,
    SOFT,
    ASYMMETRIC
}

int main(int argc, char** argv) {
	float gain = 20;
	char *typestr = "soft";

	opt.opt_float("g", "gain", &gain);
	opt.str("t", "type (hard / soft / asymmetric)", &typestr);
	opt.nargs(1, "input.wav");

	char **args = opt.parse(argc, argv);

	int type = ASYMMETRIC;
    const char *in_path = args[0];

	if (strcmp(typestr, "hard") == 0) type = HARD;
	else if (strcmp(typestr, "soft") == 0) type = SOFT;
	else if (strcmp(typestr, "asymmetric") == 0) type = ASYMMETRIC;

    wav.reader_t *in = wav.open_reader(in_path);
	if (!in) panic("!");
	wav.writer_t *out = wav.open_writer(stdout);

	double dgain = gain;
	while (wav.more(in)) {
		wav.samplef_t s = wav.read_samplef(in);
		double left = dist(type, s.left * dgain);
		double right = dist(type, s.right * dgain);
		wav.write_sample(out, left, right);
	}

	wav.close_reader(in);
	wav.close_writer(out);
    return 0;
}

double dist(int type, double x) {
	switch (type) {
		case HARD: { return hard_clip(x, 0.5); }
		case SOFT: {
			// soft clip doesn't guarantee the signal stays within the limit,
			// hence the hard clip on top.
			return hard_clip(soft_clip(x, 0.5), 1.0);
		}
		case ASYMMETRIC: { return asymmetric_clip(x, 0.6, 0.4); }
		default: { panic("!"); }
	}
}

double hard_clip(double x, double threshold) {
    if (x > threshold) return threshold;
    if (x < -threshold) return -threshold;
    return x;
}

double soft_clip(double x, double threshold) {
	// return (exp(2 * x) - 1) / (exp(2 * x) + 1);
	if (x > threshold) {
        return (2.0/3.0) * threshold + (threshold*threshold*threshold)/(3.0*x*x);
	}
    if (x < -threshold) {
        return -( (2.0/3.0) * threshold + (threshold*threshold*threshold)/(3.0*x*x) );
	}
    return x - (x*x*x)/3.0;
}

double asymmetric_clip(double x, double pos_thresh, double neg_thresh) {
    if (x > pos_thresh) return pos_thresh;
    if (x < -neg_thresh) return -neg_thresh;
    return x;
}
