#import formats/wav

int main() {
	wav.reader_t *wr = wav.open_reader("out.wav");
	if (!wr) panic("openwav failed");

	printf("format = %u\n", wr->wav.format);
    printf("channels = %u\n", wr->wav.channels);
    printf("freq = %u Hz\n", wr->wav.frequency);
    printf("bits per sample = %u\n", wr->wav.bits_per_sample);

    double t = 0;
    double dt = 1.0 / wr->wav.frequency;
    int i = 0;
	while (wav.more(wr)) {
		wav.sample_t s = wav.read_sample(wr);
        printf("%f\t%d\t%d\n", t, s.left, s.right);
        i++;
        t = dt * i;
        if (t >= 1) break;
    }
	wav.close_reader(wr);
	return 0;
}
