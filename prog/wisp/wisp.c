#import eval.c
#import opt
#import strings

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
	eval.object_t *args = eval.nil();
	paths++;
	while (paths[0] != NULL) {
		args = eval.c_cons(eval.c_strs(strings.newstr("%s", paths[0])), args);
		paths++;
	}
	eval.SET(eval.c_sym("ARGS"), args);
	eval.load_file (fid, file, 0);
	fclose(fid);
	return 0;
}
