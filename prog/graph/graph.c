#import diamondssquares.c
#import vid.c
#import opt

int main(int argc, char *argv[]) {
	opt.addcmd("vid", vid.run);
	opt.addcmd("diamonds-square", diamondssquares.run);
	return opt.dispatch(argc, argv);
}
