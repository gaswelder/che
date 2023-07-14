#import fs

int main(int argc, char *argv[]) {
	if (argc > 2) {
		fprintf(stderr, "Usage: dir [path]\n");
		return 1;
	}
	const char *path = NULL;
	if (argc > 1) {
		path = argv[1];
	} else {
		path = ".";
	}
	fs.dir_t *d = fs.dir_open(path);
	if (!d) {
		fprintf(stderr, "Couldn't read %s\n", path);
		return 1;
	}
	while (true) {
		const char *fn = fs.dir_next(d);
		if (!fn) break;
		printf("%s\n", fn);
	}
	fs.dir_close(d);
	return 0;
}
