#import midilib.c

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "subcommands: ls, render\n");
		return 1;
	}
    switch str(argv[1]) {
		case "ls": { return main_ls(argc-1, argv+1); }
		// case "render": { return main_render(argc-1, argv+1); }
	}
	fprintf(stderr, "subcommands: ls, render\n");
	return 1;
}

int main_ls(int argc, char *argv[]) {
	if (argc != 2) {
        fprintf(stderr, "usage: %s midi-file\n", argv[0]);
        return 1;
    }
    midilib.midi_t *m = midilib.open(argv[1]);
    if (!m) {
        fprintf(stderr, "failed to open %s: %s\n", argv[1], strerror(errno));
        return 1;
    }

	size_t n = 0;
	midilib.event_t *ee = NULL;
	midilib.read_file(m, &ee, &n);

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

// int main_render(int argc, char *argv[]) {
// 	if (argc != 2) {
//         fprintf(stderr, "usage: %s midi-file\n", argv[0]);
//         return 1;
//     }

// 	for (size_t i = 0; i < n; i++) {
// 		midi.event_t e = ee[i];
// 		shift(t, e.?time);

// 		if (note on) {
// 			create range
// 		}
// 	}

// 	free(events)
// }

