#import formats/wav

// mixes two waves together

int main(int argc, char *argv[]) {
	if (argc != 3) {
		fprintf(stderr, "arguments: wav-file1 wav-file2\n");
		return 1;
	}
	wav.reader_t *wr1 = wav.open_reader(argv[1]);
	if (!wr1) panic("open_reader failed");

	wav.reader_t *wr2 = wav.open_reader(argv[2]);
	if (!wr2) panic("open_reader failed");

	// FILE *out = fopen("out2.wav", "wb");
	// if (!out) panic("fopen failed");
	FILE *out = stdout;
	wav.writer_t *ww = wav.open_writer(out);

	while (true) {
		bool ok = false;
		wav.sample_t s1 = {};
		wav.sample_t s2 = {};
		if (wav.more(wr1)) {
			ok = true;
			s1 = wav.read_sample(wr1);
		}
		if (wav.more(wr2)) {
			ok = true;
			s2 = wav.read_sample(wr2);
		}
		if (!ok) break;
		wav.write_sample(ww, s1.left + s2.left, s1.right + s2.right);
	}

	wav.close_writer(ww);
	fclose(out);

	wav.close_reader(wr1);
	wav.close_reader(wr2);
	return 0;
}
