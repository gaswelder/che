#import midilib.c

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s midi-file\n", argv[0]);
        return 1;
    }
    midilib.midi_t *m = midilib.open(argv[1]);
    if (!m) {
        fprintf(stderr, "failed to open %s: %s\n", argv[1], strerror(errno));
        return 1;
    }
	midilib.read_file(m);
    return 0;
}
