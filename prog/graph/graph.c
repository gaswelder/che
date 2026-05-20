#import diamondssquares.c
#import mandelbrot.c
#import vid.c
#import opt

int main(int argc, char *argv[]) {
	opt.addcmd("vid", vid.run);
	opt.addcmd("diamonds-square", diamondssquares.run);
	opt.addcmd("mandelbrot", mandelbrot.run);
	return opt.dispatch(argc, argv);
}
