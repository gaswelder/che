#import panic

typedef {
    /*
     * 0-the file contains a single multi-channel track
     * 1-the file contains one or more simultaneous tracks (or MIDI outputs) of a sequence
     * 2-the file contains one or more sequentially independent single-track patterns
     */
    int format;

    /*
     * How many tracks the file has. Always 1 for format 0.
     */
    uint16_t ntracks;

    /*
     * Unit of time for delta timing. If the value is positive, then it
     * represents the units per beat. For example, +96 would mean 96 ticks per
     * beat. If the value is negative, delta times are in SMPTE compatible
     * units.
     */
    int division;
} midi_t;

int main() {
    FILE *f = fopen("spain-1.mid", "rb");

    midi_t midi = {};

    header_t h = header(f);
    printf("type = %s, length = %u\n", h.type, h.length);
    midi.format = (int) read16(f);
    midi.ntracks = read16(f);
    midi.division = (int) read16(f);
    printf("format = %u, ntracks = %u, ticks = %u\n", midi.format, midi.ntracks, midi.division);
    if (midi.division < 0) {
        panic("SMPTE format of division is not supported");
    }
    
    while (!feof(f)) {
        h = header(f);
        printf("type = %s, length = %u\n", h.type, h.length);
        
        if (strcmp("MTrk", h.type)) {
            panic("unknown track type: %s\n", h.type);
        }
        while (h.length > 0) {
            size_t n = trackevent(f);
            if (n > h.length) {
                panic("read %zu bytes of event while remaining is %zu", n, h.length);
            }
            h.length -= n;
        }
    }

    fclose(f);
}

size_t trackevent(FILE *f) {
    size_t bytes = 0;
    size_t time = val(f, &bytes);
    int tmp = fgetc(f);
    bytes++;
    printf("event time = %zu, tmp = %d\n", time, tmp);
    if (tmp == 0xFF) {
        bytes += metaevent(f);
    } else {
        panic("unknown event %d\n", tmp);
    }
    return bytes;
}

size_t metaevent(FILE *f) {
    size_t bytes = 0;
    
    int type = fgetc(f);
    bytes++;

    size_t length = val(f, &bytes);
    printf("meta event %d, length = %zu\n", type, length);
    while (length > 0) {
        fgetc(f);
        bytes++;
        length--;
    }

    return bytes;
}

size_t val(FILE *f, size_t *bytes) {
    size_t r = 0;
    size_t n = 0;
    while (!feof(f)) {
        n++;
        uint8_t c = fgetc(f);
        // 7 bits of every byte are the value.
        r *= 128;
        r += c & 127;
        // The 8th bit is a continuation bit.
        if (c <= 127) {
            break;
        }
    }
    *bytes = n;
    return r;
}

uint16_t read16(FILE *f) {
    uint16_t r = 0;
    for (int i = 0; i < 2; i++) {
        r = r * 256 + fgetc(f);
    }
    return r;
}

uint32_t read32(FILE *f) {
    uint32_t r = 0;
    for (int i = 0; i < 4; i++) {
        r = r * 256 + fgetc(f);
    }
    return r;
}

typedef {
    char type[5];
    uint32_t length;
} header_t;

header_t header(FILE *f) {
    header_t h = {
        .type = {0},
        .length = 0
    };

    for (int i = 0; i < 4; i++) {
        h.type[i] = fgetc(f);
    }
    h.length = read32(f);
    return h;
}
