#import cmd_create.c
#import cmd_seed.c
#import cmd_info.c
#import cmd_download.c
#import opt

int main(int argc, char *argv[]) {
	opt.addcmd("create", cmd_create.run);
	opt.addcmd("seed", cmd_seed.run);
	opt.addcmd("info", cmd_info.run);
	opt.addcmd("download", cmd_download.run);
	return opt.dispatch(argc, argv);
}
