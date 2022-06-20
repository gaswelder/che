#import panic

// https://www.recordingblogs.com/wiki/midi-controller-message
// https://www.recordingblogs.com/wiki/midi-registered-parameter-number-rpn
// https://www.recordingblogs.com/wiki/midi-meta-messages
// https://www.recordingblogs.com/wiki/midi-marker-meta-message
// https://ibex.tech/resources/geek-area/communications/midi/midi-comms
// http://www33146ue.sakura.ne.jp/staff/iz/formats/midi-event.html
// https://ccrma.stanford.edu/~craig/14q/midifile/MidiFileFormat.html

enum {
    /*
     * 0-the file contains a single multi-channel track
     * 1-the file contains one or more simultaneous tracks (or MIDI outputs) of a sequence
     * 2-the file contains one or more sequentially independent single-track patterns
     */
    FORMAT_ONE_MULTI_CHANNEL_TRACK = 0,
    FORMAT_MANY_SIMULTANEOUS_TRACKS = 1,
    FORMAT_MANY_INDEPENDENT_TRACKS = 2
};

typedef {
    /*
     * One of the FORMAT_ constants
     */
    int format;
    /*
     * How many tracks the file has.
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

typedef { char type[5]; uint32_t length; } header_t;

header_t header(FILE *f) {
    header_t h;
    for (int i = 0; i < 4; i++) {
        h.type[i] = fgetc(f);
    }
    h.length = read32(f);
    return h;
}


int main() {
    FILE *f = fopen("spain-1.mid", "rb");

    midi_t midi = {};

    /*
     * The first chunk's header has file-level metadata and the body is empty.
     */
    header_t h = header(f);
    if (strcmp("MThd", h.type)) {
        panic("Expected MThd header, got %s", h.type);
    }
    printf("type = %s, length = %u\n", h.type, h.length);

    midi.format = (int) read16(f);
    printf("format = %u\n", midi.format);
    
    midi.ntracks = read16(f);
    printf("ntracks = %u\n", midi.ntracks);

    midi.division = (int) read16(f);
    if (midi.division < 0) {
        panic("SMPTE format of division is not supported");
    }
    printf("ticks = %u\n", midi.division);

    while (!feof(f)) {
        /*
         * Read chunk header
         */
        h = header(f);
        printf("type = %s, length = %u\n", h.type, h.length);
        if (strcmp("MTrk", h.type)) {
            panic("unknown track type: %s\n", h.type);
        }
        /*
         * Assuming it's MTrk, read track events.
         */
        while (h.length > 0) {
            size_t n = trackevent(f);
            if (n > h.length) {
                panic("read %zu bytes of event while remaining is %zu", n, h.length);
            }
            h.length -= n;
        }
    }

    puts("done");
    fclose(f);
}

size_t trackevent(FILE *f) {
    size_t bytes = 0;
    size_t time = val(f, &bytes);
    int tmp = fgetc(f);
    bytes++;
    printf("event time = %zu, tmp = %d\n", time, tmp);

    // meta event | midi event | sysex event
    if (tmp == 0xFF) {
        size_t n;
        metaevent_t e = metaevent(f, &n);
        printf("%s, length = %zu\n", meta_name(e.type), e.length);
        bytes += n;
        return bytes;
    }

    if (tmp == 0xF0) {
        panic("got sysex event");
    }

    /*
     * MIDI event
     */

    // Value (Hex) 	Command 	Data bytes
    // 0x80-0x8F 	Note off 	2 (note, velocity)
    if (tmp >= 0x80 && tmp <= 0x8F) {
        int chan = tmp % 16;
        int note = fgetc(f);
        int velocity = fgetc(f);
        printf("NOTE_OFF, chan=%d note=%d vel=%d\n", chan, note, velocity);
        return bytes + 2;
    }
    // 0x90-0x9F 	Note on 	2 (note, velocity)
    // 0xA0-0xAF 	Key Pressure 	2 (note, key pressure)
    // 0xB0-0xBF 	Control Change 	2 (controller no., value)
    if (tmp >= 0xB0 && tmp <= 0xBF) {
        int chan = tmp % 16;
        int number = fgetc(f);
        int value = fgetc(f);
        printf("CONTROL_CHANGE, chan=%d number=%d value=%d\n", chan, number, value);
        return bytes + 2;
    }
    // 0xC0-0xCF 	Program Change 	1 (program no.)
    // 0xD0-0xDF 	Channel Pressure 	1 (pressure)
    // 0xE0-0xEF 	Pitch Bend 	2 (least significant byte, most significant byte)
    
    printf("0x%X\n", fgetc(f));
    printf("0x%X\n", fgetc(f));
    printf("0x%X\n", fgetc(f));
    printf("0x%X\n", fgetc(f));
    printf("0x%X\n", fgetc(f));
    printf("0x%X\n", fgetc(f));
    printf("0x%X\n", fgetc(f));
    printf("0x%X\n", fgetc(f));
    printf("0x%X\n", fgetc(f));
    printf("0x%X\n", fgetc(f));
    printf("0x%X\n", fgetc(f));
    printf("0x%X\n", fgetc(f));
    printf("0x%X\n", fgetc(f));
    printf("0x%X\n", fgetc(f));
    printf("0x%X\n", fgetc(f));
    printf("0x%X\n", fgetc(f));
    
    panic("unknown event 0x%X\n", tmp);
    return bytes + 2;
}


enum {
    META_SEQ_NUM = 0,
    META_TEXT = 1,
    META_COPYRIGHT = 2,
    META_SEQ_NAME = 3, // sequence or track name
    META_INSTRUMENT_NAME = 4,
    META_LYRIC_TEXT = 5,
    META_MARKER_TEXT = 6,
    META_CUE_POINT = 7,

    META_MIDI_CHANNEL_PREFIX_ASSIGNMENT = 0x20,
    META_END_OF_TRACK = 0x2F,
    META_TEMPO = 0x51,
    META_SMPTE_OFFSET = 0x54,
    META_TIME_SIGNATURE = 0x58,
    META_KEY_SIGNATURE = 0x59,
    META_OTHER = 0x7F // sequencer-specific
};

char *meta_name(int meta) {
    switch (meta) {
        case META_SEQ_NUM: return "META_SEQ_NUM";
        case META_TEXT: return "META_TEXT";
        case META_COPYRIGHT: return "META_COPYRIGHT";
        case META_SEQ_NAME: return "META_SEQ_NAME";
        case META_INSTRUMENT_NAME: return "META_INSTRUMENT_NAME";
        case META_LYRIC_TEXT: return "META_LYRIC_TEXT";
        case META_MARKER_TEXT: return "META_MARKER_TEXT";
        case META_CUE_POINT: return "META_CUE_POINT";
        case META_MIDI_CHANNEL_PREFIX_ASSIGNMENT: return "META_MIDI_CHANNEL_PREFIX_ASSIGNMENT";
        case META_END_OF_TRACK: return "META_END_OF_TRACK";
        case META_TEMPO: return "META_TEMPO";
        case META_SMPTE_OFFSET: return "META_SMPTE_OFFSET";
        case META_TIME_SIGNATURE: return "META_TIME_SIGNATURE";
        case META_KEY_SIGNATURE: return "META_KEY_SIGNATURE";
        case META_OTHER: return "META_OTHER";
        default: panic("unknown meta event %d", meta);
    }
    return NULL;
}

typedef {
    uint8_t type;
    size_t length;
    char data[1000];
} metaevent_t;

metaevent_t metaevent(FILE *f, size_t *rbytes) {
    metaevent_t event;
    event.type = fgetc(f);

    size_t valbytes;
    event.length = val(f, &valbytes);
    if (event.length > 1000) {
        panic("metaevent.data too small, need %zu", event.length);
    }
    for (size_t i = 0; i < event.length; i++) {
        event.data[i] = fgetc(f);
    }

    *rbytes = valbytes + 1 + event.length;
    return event;
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

