#import birthday.c
#import pixels.c

int main(int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, "usage: %s birthday|pixels\n", argv[0]);
		return 1;
	}
	switch str (argv[1]) {
		case "birthday": { return birthday.main(); }
		case "pixels": { return pixels.main(); }
	}
	fprintf(stderr, "usage: %s birthday|pixels\n", argv[0]);
	return 1;
}
