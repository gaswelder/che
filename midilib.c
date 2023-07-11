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

const char *formatname(int format) {
    switch (format) {
        case FORMAT_ONE_MULTI_CHANNEL_TRACK: { return "FORMAT_ONE_MULTI_CHANNEL_TRACK"; }
        case FORMAT_MANY_SIMULTANEOUS_TRACKS: { return "FORMAT_MANY_SIMULTANEOUS_TRACKS"; }
        case FORMAT_MANY_INDEPENDENT_TRACKS: { return "FORMAT_MANY_INDEPENDENT_TRACKS"; }
        default: { return "unknown format"; }
    }
}


typedef {
    char name[5]; // chunk's type
    uint32_t length; // chunk's length in bytes
} chunk_head_t;

chunk_head_t read_chunk_head(FILE *f) {
    chunk_head_t h = {};
    for (int i = 0; i < 4; i++) {
        h.name[i] = fgetc(f);
    }
    h.length = read32(f);
    return h;
}

pub typedef {
    FILE *f;
} midi_t;

pub midi_t *open(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    midi_t *m = calloc(1, sizeof(midi_t));
    if (!m) {
        fclose(f);
        return NULL;
    }
    m->f = f;
    load_head_chunk(m);
    return m;
}

void load_head_chunk(midi_t *m) {
    FILE *f = m->f;

    chunk_head_t h = read_chunk_head(f);
    if (strcmp("MThd", h.name)) {
        panic("expected MThd, got %s", h.name);
    }
    if (h.length != 6) {
        panic("expected length 6, got %u\n", h.length);
    }

    // the format of the MIDI file, 2 bytes.
    // 0 = single track file format
    // 1 = multiple track file format
    // 2 = multiple song file format 
    uint16_t format = read16(f);

    // the number of tracks that follow, 2 bytes.
    uint16_t n = read16(f);
    

    // a timing value specifying delta time units.
    // <division> 2 bytes
    // unit of time for delta timing.
    uint16_t division = (int) read16(f);

    printf("header chunk: len=%u, format=%s, ntracks=%u, %d ticks per beat\n",
        h.length,
        formatname(format),
        n,
        division
    );
    // If the value is positive, then it represents the units per beat.
    // For example, +96 would mean 96 ticks per beat.
    // If the value is negative, delta times are in SMPTE compatible units.
    if (division > 0) {
        // printf("(%d ticks per beat)\n", division);
    } else {
        printf("(SMPTE compatible units)\n");
    }
}

pub bool track_chunk(midi_t *m) {
    FILE *f = m->f;
    if (feof(f)) {
        return false;
    }
    // Each track chunk defines a logical track.
    // track_chunk = "MTrk" + <length> + <track_event> [+ <track_event> ...]

    chunk_head_t h = read_chunk_head(f);
    if (strcmp("MTrk", h.name)) {
        panic("expected MTrk, got %s", h.name);
    }

    // and actual event data making up the track.
    while (!feof(f)) {
        // Elapsed time (delta time) from the previous event to this event.
        int v_time = variable_length_value(f);
        int event_type = fgetc(f);

        printf("event 0x%X (+%d) ", event_type, v_time);

        // The messages from 0x80 to 0xEF are called Channel Messages because the second four bits of the command specify which channel the message affects.
        if (event_type >= 0x80 && event_type <= 0xEF) {
            int type = (event_type >> 4) & 0xF;
            int channel = event_type & 0xF;
            
            // printf("type = %x (%s), channel = %x\n", type, cmdname(type), channel);
            switch (type) {
                case 0x8: {
                    // 0x80-0x8F 	Note off 	2 (note, velocity)
                    int arg1 = fgetc(f);
                    int arg2 = fgetc(f);
                    printf("\tnote off, channel=%d note,velocity=%d,%d\n", channel, arg1, arg2);
                }
                case 0x9: {
                    // 0x90-0x9F 	Note on 	2 (note, velocity)
                    int arg1 = fgetc(f);
                    int arg2 = fgetc(f);
                    printf("\tnote on, channel=%d note,velocity=%d,%d\n", channel, arg1, arg2);
                }
                case 0xA: {
                    // 0xA0-0xAF 	Key Pressure 	2 (note, key pressure)
                    int arg1 = fgetc(f);
                    int arg2 = fgetc(f);
                    printf("\tkey pressure, channel=%d note,pressure=%d,%d\n", channel, arg1, arg2);
                }
                case 0xB: {
                    // 0xB0-0xBF 	Control Change 	2 (controller no., value)
                    int controller = fgetc(f);
                    int value = fgetc(f);
                    printf("\tcontrol change, channel=%d controller=%d value=%d\n", channel, controller, value);
                }
                case 0xC: {
                    // 0xC0-0xCF 	Program Change 	1 (program no.)
                    int arg1 = fgetc(f);
                    printf("\tprogram change, channel=%d arg=%d\n", channel, arg1);
                }
                case 0xD: {
                    // 0xD0-0xDF 	Channel Pressure 	1 (pressure)
                    int arg1 = fgetc(f);
                    printf("\tchannel pressure, channel=%d pressure=%d\n", channel, arg1);
                }
                case 0xE: {
                    // 0xE0-0xEF 	Pitch Bend 	2 (least significant byte, most significant byte)
                    int arg1 = fgetc(f);
                    int arg2 = fgetc(f);
                    printf("\tpitch bend, channel=%d args=%d,%d\n", channel, arg1, arg2);
                }
                default: {
                    panic("unhandled type %X", type);
                }
            }
            continue;
        }

        // <sysex_event>
        //     an SMF system exclusive event. 
        // System Exclusive Event
        // A system exclusive event can take one of two forms:
        // sysex_event = 0xF0 + <data_bytes> 0xF7 or sysex_event = 0xF7 + <data_bytes> 0xF7
        // In the first case, the resultant MIDI data stream would include the 0xF0. In the second case the 0xF0 is omitted. 
   

        // The messages from 0xF0 to 0xFF are called System Messages; they do not affect any particular channel. 

        // Certain MIDI events are never sent over MIDI ports, but exist in MIDI files.
        // These events are called meta events and the messages that they carry are called meta messages.
        // MIDI meta messages are messages that contains information about the MIDI sequence and that are not to be sent over MIDI ports.
        if (event_type == 0xFF) {
            // meta_event = 0xFF + <meta_type> + <v_length> + <event_data_bytes>
            int meta_type = fgetc(f); // 1 byte
            int v_length = variable_length_value(f); // var
            char data[1000] = {0};
            for (int i = 0; i < v_length; i++) {
                data[i] = fgetc(f);
                if (i >= 999) {
                    panic("data too big");
                }
            }
            printf("\t%s \tlen=%d\tdata=%s\n", meta_name(meta_type), v_length, data);
            if (meta_type == META_END_OF_TRACK) {
                printf("---------- end -----------\n");
                break;
            }
            continue;
        }
        printf("\n\n");
        printf("skipping 0x%X\n", event_type);
    }
    return true;
}

enum {
    META_SEQ_NUM = 0, // number of a sequence
    META_TEXT = 1, // some text
    META_COPYRIGHT = 2, // copyright notice
    META_SEQ_NAME = 3, // sequence or track name
    META_INSTRUMENT_NAME = 4, // current track's instrument name
    META_LYRIC_TEXT = 5, // Lyrics, usually a syllable per quarter note
    META_MARKER_TEXT = 6, // The text of a marker
    META_CUE_POINT = 7, // The text of a cue, usually to prompt for some action from the user

    META_MIDI_CHANNEL_PREFIX_ASSIGNMENT = 0x20, // A channel number (following meta events will apply to this channel)
    META_END_OF_TRACK = 0x2F,
    META_TEMPO = 0x51, // Number of microseconds per beat
    META_SMPTE_OFFSET = 0x54, // SMPTE time to denote playback offset from the beginning
    META_TIME_SIGNATURE = 0x58, // Time signature, metronome clicks, and size of a beat in 32nd notes
    META_KEY_SIGNATURE = 0x59, // A key signature
    META_OTHER = 0x7F // Something specific to the MIDI device manufacturer
};

char *meta_name(int meta) {
    switch (meta) {
        case META_SEQ_NUM: { return "META_SEQ_NUM"; }
        case META_TEXT: { return "META_TEXT"; }
        case META_COPYRIGHT: { return "META_COPYRIGHT"; }
        case META_SEQ_NAME: { return "META_SEQ_NAME"; }
        case META_INSTRUMENT_NAME: { return "META_INSTRUMENT_NAME"; }
        case META_LYRIC_TEXT: { return "META_LYRIC_TEXT"; }
        case META_MARKER_TEXT: { return "META_MARKER_TEXT"; }
        case META_CUE_POINT: { return "META_CUE_POINT"; }
        case META_MIDI_CHANNEL_PREFIX_ASSIGNMENT: { return "META_MIDI_CHANNEL_PREFIX_ASSIGNMENT"; }
        case META_END_OF_TRACK: { return "META_END_OF_TRACK"; }
        case META_TEMPO: { return "META_TEMPO"; }
        case META_SMPTE_OFFSET: { return "META_SMPTE_OFFSET"; }
        case META_TIME_SIGNATURE: { return "META_TIME_SIGNATURE"; }
        case META_KEY_SIGNATURE: { return "META_KEY_SIGNATURE"; }
        case META_OTHER: { return "META_OTHER"; }
        default: { panic("unknown meta event %d", meta); }
    }
}

int variable_length_value(FILE *f) {
    // Variable Length Values
    // A variable length value uses the low order 7 bits of a byte to represent the value or part of the value
    // The high order bit is an "escape" or "continuation" bit.
    // All but the last byte of a variable length value have the high order bit set.
    // The last byte has the high order bit cleared. The bytes always appear most significant byte first.

    // Here are some examples:

    //    Variable length              Real value
    //    0x7F                         127 (0x7F)
    //    0x81 0x7F                    255 (0xFF)
    //    0x82 0x80 0x00               32768 (0x8000)

    int result = 0;
    while (!feof(f)) {
        int c = fgetc(f);
        // 7 bits of every byte are the value.
        result *= 128;
        result += c & 127;
        // The 8th bit is a continuation bit.
        if (c <= 127) {
            break;
        }
    }
    return result;
}

// Numbers larger than one byte are placed most significant byte first.

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
