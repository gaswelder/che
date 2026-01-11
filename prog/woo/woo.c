#import formats/wav
#import opt

int main(int argc, char *argv[]) {
	size_t freq = 1200;
	opt.size("f", "tone frequency in Hz", &freq);
	opt.nargs(0, "");
	opt.parse(argc, argv);

	wav.writer_t *w = wav.open_writer(stdout);
	int nsamples = 44100 / 10;
	float hz = (float) freq;
	for (int j = 0; j < nsamples; j++) {
		float v = sinf(j * 2.0f * M_PI / 44100 * hz);
		wav.write_sample(w, v, v);
	}
	wav.close_writer(w);
	return 0;
}
