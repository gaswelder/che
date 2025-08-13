#import formats/wav

int main() {
	wav.reader_t *wr = wav.open_reader("out.wav");
	if (!wr) panic("open_reader failed");

	printf("format = %u\n", wr->wav.format);
    printf("channels = %u\n", wr->wav.channels);
    printf("freq = %u Hz\n", wr->wav.frequency);
    printf("bits per sample = %u\n", wr->wav.bits_per_sample);

	FILE *out = fopen("out2.wav", "wb");
	if (!out) panic("fopen failed");
	wav.writer_t *ww = wav.open_writer(out);

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
		wav.write_sample(ww, s.left, s.right);
    }

	wav.close_writer(ww);
	fclose(out);

	wav.close_reader(wr);
	return 0;
}
