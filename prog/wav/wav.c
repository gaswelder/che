#import formats/wav

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "specify a subcommand: info, mix\n");
		return 1;
	}
	switch str(argv[1]) {
		case "mix": { return main_mix(argc-2, argv+2); }
		case "info": { return main_info(argc-2, argv+2); }
	}
	fprintf(stderr, "unknown subcommand\n");
	return 1;
}

int main_mix(int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, "arguments: wav-file1 wav-file2\n");
		return 1;
	}
	wav.reader_t *wr1 = wav.open_reader(argv[0]);
	if (!wr1) panic("open_reader failed");

	wav.reader_t *wr2 = wav.open_reader(argv[1]);
	if (!wr2) panic("open_reader failed");

	if (wr1->wav.frequency != 44100) panic("expected frequency 44100, got %d", wr1->wav.frequency);
	if (wr2->wav.frequency != 44100) panic("expected frequency 44100, got %d", wr2->wav.frequency);

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

int main_info(int argc, char *argv[]) {
	if (argc == 0) {
		fprintf(stderr, "arguments: wav-file\n");
		return 1;
	}
	for (int i = 0; i < argc; i++) {
		wav.reader_t *r = wav.open_reader(argv[i]);
		if (!r) {
			fprintf(stderr, "failed to open '%s': %s\n", argv[i], strerror(errno));
			return 1;
		}
		printf("format: %d\n", r->wav.format);
		printf("channels: %d\n", r->wav.channels);
		printf("frequency: %d\n", r->wav.frequency);
		printf("bits_per_sample: %d\n", r->wav.bits_per_sample);
		wav.close_reader(r);
	}
	return 0;
}
