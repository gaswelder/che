#import bytereader
#import clip/vec

// https://g200kg.github.io/midi-dump/
// http://midi.teragonaudio.com/
// https://www.recordingblogs.com
// https://www.recordingblogs.com/wiki/time-division-of-a-midi-file
// https://ibex.tech/resources/geek-area/communications/midi/midi-comms
// http://www.music.mcgill.ca/~ich/classes/mumt306/StandardMIDIfileformat.html

bool SHOW_TODO = false;

void todo(char *format, ...) {
	if (!SHOW_TODO) return;
	fprintf(stderr, "# ");
	va_list args = {};
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}

// enum {
//     FORMAT_ONE_MULTI_CHANNEL_TRACK = 0,
//     FORMAT_MANY_SIMULTANEOUS_TRACKS = 1,
//     FORMAT_MANY_INDEPENDENT_TRACKS = 2
// }

typedef {
    bytereader.reader_t *r;
	uint16_t format; // one of the format constants.
	uint16_t ntracks;
	uint16_t start_beat_size; // How many ticks in a quarter note.
	uint32_t start_beat_duration; // How many microseconds in a quarter note.

	/*
	 * The MIDI spec allows to omit the status byte (event type)
	 * if the previous transmitted message had the same status.
	 * This is referred to as running status.
	 */
	int running_status;
} midi_t;

pub enum {
	END = 1,
	TEMPO,
	NOTE_ON,
	NOTE_OFF,
	TRACK_NAME,
}

pub typedef {
	int type;
	size_t t; // absolute time in ticks
	size_t t_us; // absolute time in microseconds
	uint8_t track;
	uint32_t val;

	uint8_t channel;
	uint8_t key, velocity; // for note on/off events
	char str[20];
} event_t;

pub void read_file(const char *path, event_t **ree, size_t *rn) {
	midi_t *m = openfile(path);
	if (!m) {
		panic("failed to open %s: %s\n", path, strerror(errno));
	}

	// A MIDI file has global events bucketed into tracks for human editor
	// convenience, but the events are still global. For example, a tempo change
	// event will be in one of the tracks, but it affects the entire playback.
	// So here we take a brute-force approach: read all events into one array
	// and sort them by absolute time (which is measured in ticks).
	vec.t *events = vec.new(sizeof(event_t));
	uint8_t track = 0;
	for (uint8_t i = 0; i < m->ntracks; i++) {
		read_track(m, track++, events);
    }

	// Unload the events into a local array.
	size_t n = vec.len(events);
	event_t *ee = calloc!(n, sizeof(event_t));
	for (size_t i = 0; i < n; i++) {
		event_t *e = vec.index(events, i);
		ee[i] = *e;
	}
	vec.free(events);

	// Sort events by time in ticks.
	qsort(ee, n, sizeof(event_t), bytime);

	// Calculate absolute time in microseconds for each event.
	uint16_t beat_size = m->start_beat_size;
	uint32_t beat_duration = m->start_beat_duration;
	size_t t_us = 0;
	size_t last_tick = 0;
	for (size_t i = 0; i < n; i++) {
		event_t *e = &ee[i];

		// Ticks since last event.
		size_t dticks = e->t - last_tick;
		last_tick = e->t;

		// Absolute time in microseconds.
		t_us += (dticks * beat_duration) / beat_size;
		e->t_us = t_us;

		switch (e->type) {
			case TEMPO: { beat_duration = e->val; }
		}
	}

	*ree = ee;
	*rn = n;
}

midi_t *openfile(const char *path) {
    bytereader.reader_t *r = bytereader.newreader(path);
    if (!r) {
        return NULL;
    }
    midi_t *m = calloc!(1, sizeof(midi_t));
    m->r = r;
	m->start_beat_duration = 500000; // us

    chunk_head_t h = {};

	//
	// MThd
	//
    if (!midibin_read_chunk_head(m, &h)) {
        panic("failed to read head: %s\n", strerror(errno));
    }
    if (strcmp("MThd", h.name)) {
		panic("expected MThd, got %s\n", h.name);
    }
    if (h.length != 6) {
        panic("expected length 6, got %u\n", h.length);
    }
    m->format = bytereader.read16(m->r);
    m->ntracks = bytereader.read16(m->r);
	m->start_beat_size = bytereader.read16(m->r);
    // printf("format: %s, tracks: %u, ticks per beat: %d\n", formatname(m->format), m->ntracks, m->start_beat_size );
    return m;
}

void read_track(midi_t *m, uint8_t track, vec.t *events) {
	// track_chunk = "MTrk" + <length> + <track_event> [+ <track_event> ...]
	chunk_head_t h = {};
	if (!midibin_read_chunk_head(m, &h)) {
		panic("failed to read head: %s\n", strerror(errno));
	}
	if (strcmp("MTrk", h.name)) {
		panic("expected MTrk, got %s", h.name);
	}

	// Current time in ticks.
	size_t ticks = 0;

	while (!bytereader.ended(m->r)) {
		//
		// Time since last event.
		//
		uint32_t dt = midibin_variable_length_value(m->r);
		if (dt > 0) {
			ticks += dt;
			// printf("%zu (+%d) ticks\n", ticks, dt);
		}

		// Read type
		if (m->running_status == 0 || get_status_type(bytereader.peekc(m->r)) != STATUS_UNKNOWN) {
			m->running_status = bytereader.readc(m->r);
		}

		int event_type = m->running_status;
		bool more = false;
		event_t e = {};
		switch (get_status_type(m->running_status)) {
			case STATUS_META: {
				e = read_meta_message(m);
				e.t = ticks;
				e.track = track;
				bool ended = e.type == END;
				more = !ended;
				if (e.type != 0) {
					vec.push(events, &e);
				}
			}
			case STATUS_CHANNEL: {
				e = read_channel_message(m);
				e.t = ticks;
				e.track = track;
				more = true;
				if (e.type != 0) {
					vec.push(events, &e);
				}
			}
			default: {
				panic("(?) message 0x%X\n", event_type);
			}
		}
		if (!more) {
			break;
		}
	}
}

int bytime(const void *a, *b) {
	const event_t *e1 = a;
	const event_t *e2 = b;
	if (e1->t < e2->t) return -1;
	if (e2->t < e1->t) return 1;
	return 0;
}

event_t read_meta_message(midi_t *m) {
	int type = bytereader.readc(m->r);
	uint32_t len = midibin_variable_length_value(m->r);

	uint8_t data[1000] = {};
	for (uint32_t i = 0; i < len; i++) {
		int ch = bytereader.readc(m->r);
		if (ch < 0) panic("!");
		data[i] = ch;
		if (i >= 999) {
			panic("data too big");
		}
	}

	event_t e = {};

	switch (type) {
		case META_TEMPO: {
			if (len != 3) panic("expected tempo len of 3, got %d", len);
			e.val = (data[0] << 16) + (data[1] << 8) + data[2];
			e.type = TEMPO;
		}
		case META_SEQ_NAME: {
			e.type = TRACK_NAME;
			strcpy(e.str, (char *) data);
		}
		case META_END_OF_TRACK: { e.type = END; }
		case META_TIME_SIGNATURE: { cmd_meta_time_signature(data); }
		case META_MARKER_TEXT: { todo("label: %s\n", (char *) data); }
		default: {
			panic("META\t%s \t(%d bytes)\t\"%s\"\n", meta_name(type), len, (char*)data);
		}
	}
	return e;
}

// If one is not specified 4/4, 24, 8 should be assumed.
void cmd_meta_time_signature(uint8_t idata[]) {
	// Time signature is <numerator> / 2^<power>.
	// For "4/4" numerator = 4, power = 2.
	int numerator = idata[0];
	int power = idata[1];

	// How many clock signals there are in one metronome click.
	// Clock signals come at a rate of 24 signals per quarter-note.
	// 24 = click once every quarter-note, 48 = click once every half-note.
	int ticks_per_click = idata[2];

	// the number of 32nd notes per 24 MIDI clock signals.
	// This value is usually 8 because there are usually 8 32nd notes in a quarter-note.
	int n32th_per_beat = idata[3];
	todo("Time signature: %d/2^%d, metronome clicks once every %d midi clocks, %d 32th notes per beat\n",
		numerator, power, ticks_per_click, n32th_per_beat);
}

event_t read_channel_message(midi_t *m) {
	event_t e = {};

	int event_type = m->running_status;

	// The second four bits specify which channel the message affects.
	int type = (event_type >> 4) & 0xF;
	e.channel = event_type & 0xF;

	// printf("type = %x (%s), channel = %x\n", type, cmdname(type), channel);
	switch (type) {
		// 0x80-0x8F - stop a note (note, velocity)
		case 0x8: {
			e.type = NOTE_OFF;
			e.key = bytereader.readc(m->r); // which key to release
			e.velocity = bytereader.readc(m->r); // how fast/hard the key was released (0..127)
		}
		// 0x90-0x9F - start a note (note, velocity)
		case 0x9: {
			e.type = NOTE_ON;
			e.key = bytereader.readc(m->r); // which of the 128 MIDI keys is being played (0..127)
			e.velocity = bytereader.readc(m->r); // volume and intensity (0..127)
		}
		// 0xB0-0xBF knob setting (knob, value)
		case 0xB: {
			int knob = bytereader.readc(m->r);
			int value = bytereader.readc(m->r);
			todo("knob value\tchannel=%d\tknob=%s\tval=%d\n", e.channel, controller_name(knob), value);
		}
		// 0xC0-0xCF set instrument (id)
		case 0xC: {
			int arg1 = bytereader.readc(m->r);
			todo("set instr\tchannel=%d arg=%d\n", e.channel, arg1);
		}


		// case 0xA: {
		// 	// 0xA0-0xAF 	Key Pressure 	2 (note, key pressure)
		// 	int arg1 = bytereader.readc(m->r);
		// 	int arg2 = bytereader.readc(m->r);
		// 	printf("key pressure, channel=%d note,pressure=%d,%d\n", channel, arg1, arg2);
		// }
		// case 0xD: {
		// 	// 0xD0-0xDF 	Channel Pressure 	1 (pressure)
		// 	int arg1 = bytereader.readc(m->r);
		// 	printf("\tchannel pressure, channel=%d pressure=%d\n", channel, arg1);
		// }

		// 0xE0-0xEF pitch bend (val)
		case 0xE: {
			// least significant byte, most significant byte
			int arg1 = bytereader.readc(m->r);
			int arg2 = bytereader.readc(m->r);
			int bend = arg2 * 256 + arg1;
			// 0 = 2 semitones down
			// 16383 = 2 semitones up
			todo("pitch bend\tchannel=%d bend=%d\n", e.channel, bend);
		}
		default: {
			panic("unhandled type %X", type);
		}
	}
	return e;
}

// -------------- midi binary packaging ------------

typedef {
    char name[5]; // chunk's type
    uint32_t length; // chunk's length in bytes
} chunk_head_t;

bool midibin_read_chunk_head(midi_t *m, chunk_head_t *h) {
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

uint32_t midibin_variable_length_value(bytereader.reader_t *r) {
	// Variable-length encoding. 1 bit is the continuation bit, 7 bits are data.
    // 0x7F           = 127 (0x7F)
    // 0x81 0x7F      = 255 (0xFF)
    // 0x82 0x80 0x00 = 32768 (0x8000)

    uint32_t result = 0;
    while (true) {
        int c = bytereader.readc(r);
        if (c == EOF) {
            break;
        }
		if (c < 0) panic("!");
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


// --------------- tables -------------------

/*
 * Message types
 */
enum {
	STATUS_UNKNOWN = 0,
	STATUS_META,
	STATUS_CHANNEL,
	STATUS_SYSTEM
};

int get_status_type(int c) {
	if (c == 0xFF) return STATUS_META;
	if (c >= 0x80 && c <= 0xEF) return STATUS_CHANNEL;
	if (c >= 0xF0 && c <= 0xFF) return STATUS_SYSTEM;
	return STATUS_UNKNOWN;
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

// const char *formatname(int format) {
//     switch (format) {
//         case FORMAT_ONE_MULTI_CHANNEL_TRACK: { return "0, single track"; }
//         case FORMAT_MANY_SIMULTANEOUS_TRACKS: { return "1, multiple tracks"; }
//         case FORMAT_MANY_INDEPENDENT_TRACKS: { return "2, multiple songs"; }
//         default: { return "unknown format"; }
//     }
// }


const char *CONTROLLER_NAMES[] = {
	"Bank Select", // 0 (0x00)
	"Modulation", // 1 (0x01)
	"Breath Controller", // 2 (0x02)
	NULL,
	"Foot Controller", // 4 (0x04)
	"Portamento Time", // 5 (0x05)
	"Data Entry (MSB)", // 6 (0x06)
	"Main Volume", // 7 (0x07)
	"Balance", // 8 (0x08)
	NULL,
	"Pan", // 10 (0x0A)
	"Expression Controller", // 11 (0x0B)
	"Effect Control 1", // 12 (0x0C)
	"Effect Control 2", // 13 (0x0D)
	NULL,
	NULL,
	"Generar-purpose Controller 1", // 16-19 (0x10-0x13) 	General-Purpose Controllers 1-4
	"Generar-purpose Controller 2",
	"Generar-purpose Controller 3",
	"Generar-purpose Controller 4", // 19
	// 32-63 (0x20-0x3F) 	LSB for controllers 0-31
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, // 30
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, // 41
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, // 52
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, // 63
	"Damper pedal (sustain)", // 64 (0x40)
	"Portamento", // 65 (0x41)
	"Sostenuto", // 66 (0x42)
	"Soft Pedal", // 67 (0x43)
	"Legato Footswitch", // 68 (0x44)
	"Hold 2", // 69 (0x45)
	"Sound Controller 1 (default: Timber Variation)", // 70 (0x46)
	"Sound Controller 2 (default: Timber/Harmonic Content)", // 71 (0x47)
	"Sound Controller 3 (default: Release Time)", // 72 (0x48)
	"Sound Controller 4 (default: Attack Time)", // 73 (0x49)
	// 74-79 (0x4A-0x4F) 	Sound Controller 6-10
	"Sound Controller 5", // 74
	"Sound Controller 6", // 75
	"Sound Controller 7", // 76
	"Sound Controller 8", // 77
	"Sound Controller 9", // 78
	"Sound Controller 10", // 79
	// 80-83 (0x50-0x53) 	General-Purpose Controllers 5-8
	"General-purpose Controller 5", // 80
	"General-purpose Controller 6", // 81
	"General-purpose Controller 7", // 82
	"General-purpose Controller 8", // 83
	"Portamento Control", // 84 (0x54)
	NULL, NULL, NULL, NULL, NULL, NULL, // 90
	"Effects 1 Depth (formerly External Effects Depth)", // 91 (0x5B)
	"Effects 2 Depth (formerly Tremolo Depth)", // 92 (0x5C)
	"Effects 3 Depth (formerly Chorus Depth)", // 93 (0x5D)
	"Effects 4 Depth (formerly Celeste Detune)", // 94 (0x5E)
	"Effects 5 Depth (formerly Phaser Depth)", // 95 (0x5F)
	"Data Increment", // 96 (0x60)
	"Data Decrement", // 97 (0x61)
	"Non-Registered Parameter Number (LSB)", // 98 (0x62)
	"Non-Registered Parameter Number (MSB)", // 99 (0x63)
	"Registered Parameter Number (LSB)", // 100 (0x64)
	"Registered Parameter Number (MSB)", // 101 (0x65),
	// 121-127 (0x79-0x7F) 	Mode Messages
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, // 112
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, // 123
	NULL, NULL, NULL, NULL // 127
};

const char *controller_name(int controller) {
	return CONTROLLER_NAMES[controller];
}