#import attractor.c
#import diamondssquares.c
#import mandelbrot.c
#import thorn.c
#import opt

int main(int argc, char *argv[]) {
	opt.addcmd("attractor", attractor.run);
	opt.addcmd("thorn", thorn.run);
	opt.addcmd("diamonds-square", diamondssquares.run);
	opt.addcmd("mandelbrot", mandelbrot.run);
	return opt.dispatch(argc, argv);
}
