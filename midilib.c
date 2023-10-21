#import bytereader

// https://www.recordingblogs.com/wiki/midi-program-change-message
// https://www.recordingblogs.com/wiki/midi-controller-message
// https://www.recordingblogs.com/wiki/midi-registered-parameter-number-rpn
// https://www.recordingblogs.com/wiki/midi-meta-messages
// https://www.recordingblogs.com/wiki/midi-marker-meta-message
// https://www.recordingblogs.com/wiki/time-division-of-a-midi-file
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
        case FORMAT_ONE_MULTI_CHANNEL_TRACK: { return "0, single track"; }
        case FORMAT_MANY_SIMULTANEOUS_TRACKS: { return "1, multiple tracks"; }
        case FORMAT_MANY_INDEPENDENT_TRACKS: { return "2, multiple songs"; }
        default: { return "unknown format"; }
    }
}

typedef {
    char name[5]; // chunk's type
    uint32_t length; // chunk's length in bytes
} chunk_head_t;

bool read_chunk_head(midi_t *m, chunk_head_t *h) {
    for (int i = 0; i < 4; i++) {
        int c = bytereader.readc(m->r);
        if (c == EOF) {
            return false;
        }
        h->name[i] = c;
    }
    h->length = bytereader.read32(m->r);
    return h;
}

pub typedef {
    bytereader.reader_t *r;
} midi_t;

pub midi_t *open(const char *path) {
    bytereader.reader_t *r = bytereader.newreader(path);
    if (!r) {
        return NULL;
    }
    midi_t *m = calloc(1, sizeof(midi_t));
    if (!m) {
        bytereader.freereader(r);
        return NULL;
    }
    m->r = r;
    load_head_chunk(m);
    return m;
}

void load_head_chunk(midi_t *m) {
    chunk_head_t h = {};
    if (!read_chunk_head(m, &h)) {
        printf("failed to read head: %s\n", strerror(errno));
        return;
    }
    if (strcmp("MThd", h.name)) {
        panic("expected MThd, got %s", h.name);
    }
    if (h.length != 6) {
        panic("expected length 6, got %u\n", h.length);
    }

    uint16_t format = bytereader.read16(m->r);
    uint16_t ntracks = bytereader.read16(m->r);

    // a timing value specifying delta time units.
    // <division> 2 bytes
    // unit of time for delta timing.
    uint16_t division = (int) bytereader.read16(m->r);

    printf("header chunk: bytes: %u, format: %s, tracks: %u, ticks per beat: %d\n",
        h.length,
        formatname(format),
        ntracks,
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
    // Each track chunk defines a logical track.
    // track_chunk = "MTrk" + <length> + <track_event> [+ <track_event> ...]

    chunk_head_t h = {};
    if (!read_chunk_head(m, &h)) {
        printf("failed to read head: %s\n", strerror(errno));
        return false;
    }
    if (strcmp("MTrk", h.name)) {
        panic("expected MTrk, got %s", h.name);
    }

    // and actual event data making up the track.
    while (!bytereader.ended(m->r)) {
        // Elapsed time (delta time) from the previous event to this event.
        int v_time = variable_length_value(m->r);
		printf("time +%d:\t", v_time);

        int event_type = bytereader.readc(m->r);

        // 0xFF - just file meta, not to be sent to midi ports.
        if (event_type == 0xFF) {
            bool ended = read_meta_message(m);
			if (ended) {
				break;
			}
            continue;
        }

        // 0x80..0xEF - channel messages
        if (event_type >= 0x80 && event_type <= 0xEF) {
			read_channel_message(m, event_type);
            continue;
        }

        // <sysex_event>
        //     an SMF system exclusive event. 
        // System Exclusive Event
        // A system exclusive event can take one of two forms:
        // sysex_event = 0xF0 + <data_bytes> 0xF7 or sysex_event = 0xF7 + <data_bytes> 0xF7
        // In the first case, the resultant MIDI data stream would include the 0xF0. In the second case the 0xF0 is omitted. 
   

        // The messages from 0xF0 to 0xFF are called System Messages; they do not affect any particular channel. 

        
        printf("(?) skipping 0x%X\n", event_type);
    }
    return true;
}

bool read_meta_message(midi_t *m) {
	int meta_type = bytereader.readc(m->r);
	int v_length = variable_length_value(m->r);
	char data[1000] = {0};
	unsigned data_as_number = 0;
	int idata[1000] = {0};
	for (int i = 0; i < v_length; i++) {
		int ch = bytereader.readc(m->r);
		// printf("- 0x%x\n", ch);
		data_as_number *= 256;
		data_as_number += ch;
		data[i] = ch;
		idata[i] = ch;
		if (i >= 999) {
			panic("data too big");
		}
	}
	switch (meta_type) {
		case META_TEMPO: {
			printf("Tempo: %u us per beat\n", data_as_number);
		}
		case META_SEQ_NAME: {
			printf("Track name: \"%s\"\n", data);
		}
		case META_END_OF_TRACK: {
			printf("---------- end of track -----------\n");
		}
		case META_TIME_SIGNATURE: {
			int numerator = idata[0];
			int power = idata[1];
			int ticks_per_click = idata[2];
			int n32th_per_beat = idata[3];
			printf("Time signature: %d/2^%d, metronome clicks once every %d midi clocks, %d 32th notes per beat\n", numerator, power, ticks_per_click, n32th_per_beat);
		}
		default: {
			printf("META\t%s \t(%d bytes)\t\"%s\"\n", meta_name(meta_type), v_length, data);
		}
	}
	return meta_type == META_END_OF_TRACK;
}

void read_channel_message(midi_t *m, int event_type) {
	// The second four bits specify which channel the message affects.
	int type = (event_type >> 4) & 0xF;
	int channel = event_type & 0xF;
	
	// printf("type = %x (%s), channel = %x\n", type, cmdname(type), channel);
	switch (type) {
		case 0x8: {
			// 0x80-0x8F 	Note off 	2 (note, velocity)
			int arg1 = bytereader.readc(m->r);
			int arg2 = bytereader.readc(m->r);
			printf("\tnote off\tchannel=%d\tnote=%d\tvelocity=%d\n", channel, arg1, arg2);
		}
		case 0x9: {
			// 0x90-0x9F - play a note on channel (0..F)
			int pitch = bytereader.readc(m->r);
			int velocity = bytereader.readc(m->r);
			printf("\tnote on \tchannel=%d\tnote=%d\tvelocity=%d\n", channel, pitch, velocity);
		}
		case 0xA: {
			// 0xA0-0xAF 	Key Pressure 	2 (note, key pressure)
			int arg1 = bytereader.readc(m->r);
			int arg2 = bytereader.readc(m->r);
			printf("\tkey pressure, channel=%d note,pressure=%d,%d\n", channel, arg1, arg2);
		}
		case 0xB: {
			// 0xB0-0xBF 	Control Change 	2 (controller no., value)
			int controller = bytereader.readc(m->r);
			int value = bytereader.readc(m->r);
			printf("\tcontrol change, channel=%d controller=%d value=%d\n", channel, controller, value);
		}
		case 0xC: {
			// 0xC0-0xCF 	Program Change 	1 (program no.)
			int arg1 = bytereader.readc(m->r);
			printf("\tprogram change, channel=%d arg=%d\n", channel, arg1);
		}
		case 0xD: {
			// 0xD0-0xDF 	Channel Pressure 	1 (pressure)
			int arg1 = bytereader.readc(m->r);
			printf("\tchannel pressure, channel=%d pressure=%d\n", channel, arg1);
		}
		case 0xE: {
			// 0xE0-0xEF bend the pitch on channel (0..F)
			// (least significant byte, most significant byte)
			int arg1 = bytereader.readc(m->r);
			int arg2 = bytereader.readc(m->r);
			int bend = arg2 * 256 + arg1;
			// 0 = 2 semitones down
			// 16383 (or 16384?) = 2 semitones up
			printf("\tpitch bend, channel=%d bend=%d\n", channel, bend);
		}
		default: {
			panic("unhandled type %X", type);
		}
	}
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
        case META_SEQ_NAME: { return "Track name"; }
        case META_INSTRUMENT_NAME: { return "META_INSTRUMENT_NAME"; }
        case META_LYRIC_TEXT: { return "META_LYRIC_TEXT"; }
        case META_MARKER_TEXT: { return "META_MARKER_TEXT"; }
        case META_CUE_POINT: { return "META_CUE_POINT"; }
        case META_MIDI_CHANNEL_PREFIX_ASSIGNMENT: { return "META_MIDI_CHANNEL_PREFIX_ASSIGNMENT"; }
        case META_END_OF_TRACK: { return "META_END_OF_TRACK"; }
        case META_TEMPO: { return "Tempo (microseconds per beat)"; }
        case META_SMPTE_OFFSET: { return "META_SMPTE_OFFSET"; }
        case META_TIME_SIGNATURE: { return "META_TIME_SIGNATURE"; }
        case META_KEY_SIGNATURE: { return "META_KEY_SIGNATURE"; }
        case META_OTHER: { return "META_OTHER"; }
        default: { panic("unknown meta event %d", meta); }
    }
}

int variable_length_value(bytereader.reader_t *r) {
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
    while (true) {
        int c = bytereader.readc(r);
        if (c == EOF) {
            break;
        }
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
