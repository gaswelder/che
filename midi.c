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

    while (true) {
        if (!midilib.track_chunk(m)) {
            break;
        }
    }
    (void) m;
    return 0;
}
