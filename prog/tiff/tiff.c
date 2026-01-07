#import cmd_dump.c
#import cmd_info.c

int main(int argc, char *argv[]) {
	if (argc >= 2) {
		switch str (argv[1]) {
			case "info": { return cmd_info.run(argc - 1, argv + 1); }
			case "dump": { return cmd_dump.run(argc - 1, argv + 1); }
		}
	}
	fprintf(stderr, "subcommands: info, dump\n");
	return 1;
}
