#import eval.c
#import opt

bool force_interaction = false;

int main (int argc, char **argv) {
	opt.flag("i", "force interaction mode", &force_interaction);
	char **paths = opt.parse(argc, argv);
	eval.wisp_init();

	if (paths[0] == NULL) {
		eval.repl();
		return 0;
	}
	
	// open script file
	char *file = paths[0];
	FILE *fid = fopen (file, "r");
	if (fid == NULL) {
		fprintf (stderr, "error: could not load %s: %s\n", file, strerror (errno));
		return 1;
	}

	/* expose argv to wisp program */
	eval.val_t *args = eval.nil();
	paths++;
	while (paths[0] != NULL) {
		args = eval.newcons(eval.newstring(paths[0]), args);
		paths++;
	}
	eval.SET(eval.symbol("ARGS"), args);
	eval.load_file (fid, file, 0);
	fclose(fid);
	return 0;
}
