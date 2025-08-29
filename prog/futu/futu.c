#import cmd_ir.c
#import cmd_render.c
#import midilib.c
#import opt

int main(int argc, char *argv[]) {
	opt.addcmd("ir", cmd_ir.run);
	opt.addcmd("ls", main_ls);
	opt.addcmd("render", cmd_render.run);
	return opt.dispatch(argc, argv);
}

int main_ls(int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, "usage: %s midi-file\n", argv[0]);
		return 1;
	}

	size_t n = 0;
	midilib.event_t *ee = NULL;
	midilib.read_file(argv[1], &ee, &n);

	for (size_t i = 0; i < n; i++) {
		midilib.event_t *e = &ee[i];

		printf("%f s\ttrack=%d\t", (double)e->t_us/1e6, e->track);
		switch (e->type) {
			case midilib.END: {
				printf("end of track\n");
			}
			case midilib.NOTE_OFF: {
				printf("note off\tchannel=%d\tnote=%d\tvelocity=%d\n", e->channel, e->key, e->velocity);
			}
			case midilib.NOTE_ON: {
				printf("note on \tchannel=%d\tnote=%d\tvelocity=%d\n", e->channel, e->key, e->velocity);
			}
			case midilib.TEMPO: {
				printf("set tempo\t%u us per quarter note\n", e->val);
			}
			case midilib.TRACK_NAME: {
				printf("track name: \"%s\"\n", e->str);
			}
			default: {
				panic("?");
			}
		}
	}

	free(ee);
	return 0;
}

