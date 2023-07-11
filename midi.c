#import midilib.c

int main() {
    midilib.midi_t *m = midilib.open("spain-1.mid");
    while (true) {
        if (!midilib.track_chunk(m)) {
            break;
        }
    }
    (void) m;
    return 0;
}
