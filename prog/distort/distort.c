#import formats/wav

enum {
    HARD,
    SOFT,
    ASYMMETRIC
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "arguments: input.wav hard|soft|asymmetric\n");
        return 1;
    }

	double gain = 20;
	int type = ASYMMETRIC;
    const char* in_path = argv[1];

	if (argc == 3) {
		const char* type_str = argv[2];
		if (strcmp(type_str, "hard") == 0) type = HARD;
		else if (strcmp(type_str, "soft") == 0) type = SOFT;
		else if (strcmp(type_str, "asymmetric") == 0) type = ASYMMETRIC;
		else { fprintf(stderr, "Unknown distortion type\n"); return 1; }
	}

    wav.reader_t *in = wav.open_reader(in_path);
	if (!in) panic("!");
	wav.writer_t *out = wav.open_writer(stdout);

	while (wav.more(in)) {
		wav.samplef_t s = wav.read_samplef(in);
		double left = dist(type, s.left * gain);
		double right = dist(type, s.right * gain);
		if (left > 1) {
			left = 1;
		}
		if (right > 1) {
			right = 1;
		}
		wav.write_sample(out, left, right);
	}

	wav.close_reader(in);
	wav.close_writer(out);
    return 0;
}


double dist(int type, double x) {
	switch (type) {
		case HARD: { return hard_clip(x, 0.5); }
		case SOFT: { return soft_clip(x, 0.5f); }
		case ASYMMETRIC: { return asymmetric_clip(x, 0.6f, 0.4f); }
		default: { panic("!"); }
	}
}

double hard_clip(double x, double threshold) {
    if (x > threshold) return threshold;
    if (x < -threshold) return -threshold;
    return x;
}

double soft_clip(double x, double threshold) {
    if (x > threshold) {
        return (2.0/3.0) * threshold + (threshold*threshold*threshold)/(3.0*x*x);
	}
    if (x < -threshold) {
        return -(2.0/3.0) * threshold + (threshold*threshold*threshold)/(3.0*x*x);
	}
    return x - (x*x*x)/3.0;
}

double asymmetric_clip(double x, double pos_thresh, double neg_thresh) {
    if (x > pos_thresh) return pos_thresh;
    if (x < -neg_thresh) return -neg_thresh;
    return x;
}
