#import attractor.c
#import thorn.c
#import diamondssquares.c
#import mandelbrot.c

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "specify the generator as an argument:\n");
		fprintf(stderr, "- attractor\n");
		fprintf(stderr, "- thorn\n");
		fprintf(stderr, "- diamonds-squares\n");
		fprintf(stderr, "- mandelbrot\n");
		return 1;
	}
	switch str (argv[1]) {
		case "attractor": { return attractor.run(argc-1, argv+1); }
		case "thorn": { return thorn.run(argc-1, argv+1); }
		case "diamonds-squares": { return diamondssquares.run(argc-1, argv+1); }
		case "mandelbrot": { return mandelbrot.run(argc-1, argv+1); }
	}
	fprintf(stderr, "unknown generator: %s\n", argv[1]);
	return 1;
}
