#import formats/wav

int main() {
	wav.writer_t *w = wav.open_writer(stdout);
	int nsamples = 44100 / 10;
	float hz = 1000;
	for (int j = 0; j < nsamples; j++) {
		float v = sinf(j * 2.0f * M_PI / 44100 * hz); // * envelope;
		wav.write_sample(w, v, v);
	}
	wav.close_writer(w);
	return 0;
}
