#import create.c
#import print.c

int main(int argc, char *argv[]) {
    if (argc < 2) {
		fprintf(stderr, "subcommands: print, create\n");
		return 1;
	}
    if (strcmp(argv[1], "print") == 0) return print.cmd(argc-1, argv+1);
	if (strcmp(argv[1], "create") == 0) return create.cmd(argc-1, argv+1);
	fprintf(stderr, "unknown subcommand: %s\n", argv[1]);
	return 1;
}
