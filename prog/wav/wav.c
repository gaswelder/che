#import formats/wav
#import opt
#import sound

int main(int argc, char *argv[]) {
	opt.addcmd("mix", main_mix);
	opt.addcmd("info", main_info);
	opt.addcmd("dump", main_dump);
	return opt.dispatch(argc, argv);
}

int main_mix(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "arguments: wav-files\n");
		return 1;
	}
	int nfiles = argc - 1;

	wav.reader_t **readers = calloc!(argc, sizeof(wav.reader_t *));
	for (int i = 0; i < nfiles; i++) {
		const char *path = argv[i+1];
		wav.reader_t *wr = wav.open_reader(path);
		if (!wr) {
			fprintf(stderr, "failed to open '%s': %s\n", path, strerror(errno));
			return 1;
		}
		if (wr->wav.frequency != 44100) {
			panic("%s: expected frequency 44100, got %d", path, wr->wav.frequency);
		}
		readers[i] = wr;
	}

	wav.writer_t *ww = wav.open_writer(stdout);

	while (true) {
		bool ok = false;
		double left = 0;
		double right = 0;
		sound.samplef_t s = {};
		for (int i = 0; i < nfiles; i++) {
			wav.reader_t *wr = readers[i];
			if (wav.more(wr)) {
				ok = true;
				s = wav.read_samplef(wr);
				left += s.left;
				right += s.right;
			}
		}
		left /= nfiles;
		right /= nfiles;
		if (!ok) break;
		wav.write_sample(ww, left, right);
	}

	wav.close_writer(ww);
	for (int i = 0; i < nfiles; i++) {
		wav.close_reader(readers[i]);
	}
	free(readers);
	return 0;
}

int main_info(int argc, char *argv[]) {
	if (argc == 1) {
		fprintf(stderr, "arguments: wav-file\n");
		return 1;
	}
	for (int i = 1; i < argc; i++) {
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
	if (argc != 2) {
		fprintf(stderr, "arguments: wav-file\n");
		return 1;
	}
	wav.reader_t *r = wav.open_reader(argv[1]);
	if (!r) {
		fprintf(stderr, "failed to open '%s': %s\n", argv[1], strerror(errno));
		return 1;
	}
	while (wav.more(r)) {
		sound.samplef_t s = wav.read_samplef(r);
		printf("%f\t%f\n", s.left, s.right);
	}
	return 0;
}
