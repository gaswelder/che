#import cmd_create.c
#import cmd_seed.c
#import cmd_info.c
#import cmd_download.c

typedef int mainfunc_t (int, char**);

typedef {
	const char *cmd;
	mainfunc_t *f;
} cmd_t;

cmd_t cmds[] = {
	{"create", cmd_create.run},
	{"seed", cmd_seed.run},
	{"info", cmd_info.run},
	{"download", cmd_download.run},
	{"", NULL}
};

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "arguments: <command>\n");
		list();
		return 1;
	}
	for (cmd_t *c = cmds; c->f != NULL; c++) {
		if (!strcmp(argv[1], c->cmd)) return c->f(argc-1, argv+1);
	}
	fprintf(stderr, "unknown command: %s\n", argv[1]);
	list();
	return 1;
}

void list() {
	fprintf(stderr, "available commands:\n");
	for (cmd_t *c = cmds; c->f != NULL; c++) {
		printf("- %s\n", c->cmd);
	}
}
