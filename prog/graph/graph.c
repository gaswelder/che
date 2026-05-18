#import attractor.c
#import diamondssquares.c
#import mandelbrot.c
#import thorn.c
#import ikeda.c
#import opt

int main(int argc, char *argv[]) {
	opt.addcmd("attractor", attractor.run);
	opt.addcmd("thorn", thorn.run);
	opt.addcmd("diamonds-square", diamondssquares.run);
	opt.addcmd("mandelbrot", mandelbrot.run);
	opt.addcmd("ikeda", ikeda.run);
	return opt.dispatch(argc, argv);
}
