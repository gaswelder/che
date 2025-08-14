#import formats/wav

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "specify a subcommand: info, mix, dump\n");
		return 1;
	}
	switch str(argv[1]) {
		case "mix": { return main_mix(argc-2, argv+2); }
		case "info": { return main_info(argc-2, argv+2); }
		case "dump": { return main_dump(argc-2, argv+2); }
	}
	fprintf(stderr, "unknown subcommand\n");
	return 1;
}

int main_mix(int argc, char *argv[]) {
	if (argc < 1) {
		fprintf(stderr, "arguments: wav-files\n");
		return 1;
	}

	wav.reader_t **readers = calloc!(argc, sizeof(wav.reader_t *));
	for (int i = 0; i < argc; i++) {
		wav.reader_t *wr = wav.open_reader(argv[i]);
		if (!wr) {
			fprintf(stderr, "failed to open '%s': %s\n", argv[i], strerror(errno));
			return 1;
		}
		if (wr->wav.frequency != 44100) {
			panic("%s: expected frequency 44100, got %d", argv[i], wr->wav.frequency);
		}
		readers[i] = wr;
	}

	wav.writer_t *ww = wav.open_writer(stdout);

	double scale = 1 << 15;
	while (true) {
		bool ok = false;
		double left = 0;
		double right = 0;
		wav.samplef_t s = {};
		for (int i = 0; i < argc; i++) {
			wav.reader_t *wr = readers[i];
			if (wav.more(wr)) {
				ok = true;
				s = wav.read_samplef(wr);
				left += s.left;
				right += s.right;
			}
		}
		if (!ok) break;
		wav.write_sample(ww, (int) (scale * left), (int) (scale * right));
	}

	wav.close_writer(ww);
	for (int i = 0; i < argc; i++) {
		wav.close_reader(readers[i]);
	}
	free(readers);
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

int main_dump(int argc, char *argv[]) {
	if (argc != 1) {
		fprintf(stderr, "arguments: wav-file\n");
		return 1;
	}
	wav.reader_t *r = wav.open_reader(argv[0]);
	if (!r) {
		fprintf(stderr, "failed to open '%s': %s\n", argv[0], strerror(errno));
		return 1;
	}
	while (wav.more(r)) {
		wav.samplef_t s = wav.read_samplef(r);
		printf("%f\t%f\n", s.left, s.right);
	}
	return 0;
}
