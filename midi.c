#import midilib.c

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <midi-path>\n", argv[0]);
        return 1;
    }

    midilib.midi_t *m = midilib.open(argv[1]);
    if (!m) {
        fprintf(stderr, "failed to open %s: %s\n", argv[1], strerror(errno));
        return 1;
    }

	if (m->delta_time_unit == midilib.TICKS_PER_BEAT) {
		printf("time unit = %d ticks per beat\n", m->delta_time_value);
	} else {
		printf("time unit = %d frames per second\n", m->delta_time_value);
	}

	midilib.read_file(m);
    (void) m;
    return 0;
}
