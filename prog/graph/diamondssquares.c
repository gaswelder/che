#import formats/png
#import frac/diamondsquare.c
#import image
#import opt

pub int run(int argc, char *argv[]) {
	opt.nargs(1, "<outpath.png>");
	char **args = opt.parse(argc, argv);

	// Must be 2^n + 1.
	int size = 513;

	image.image_t *img = image.new(size, size);
	diamondsquare.draw(img, size);
	png.write(img, args[0], png.PNG_RGB);
	image.free(img);
	return 0;
}
