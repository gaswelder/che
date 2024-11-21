#import formats/wav

int main() {
	wav.reader_t *wr = wav.open_reader("out.wav");
	if (!wr) panic("open_reader failed");

	printf("format = %u\n", wr->wav.format);
    printf("channels = %u\n", wr->wav.channels);
    printf("freq = %u Hz\n", wr->wav.frequency);
    printf("bits per sample = %u\n", wr->wav.bits_per_sample);

	wav.writer_t *ww = wav.open_writer("out2.wav");
	if (!ww) panic("open_writer failed");

    double t = 0;
    double dt = 1.0 / wr->wav.frequency;
    int i = 0;
	while (wav.more(wr)) {
		i++;
        t = dt * i;
		wav.sample_t s = wav.read_sample(wr);
		if (t <= 1) {
			printf("%f\t%d\t%d\n", t, s.left, s.right);
		}
		wav.write_sample(ww, s);
    }

	wav.close_writer(ww);
	wav.close_reader(wr);
	return 0;
}
