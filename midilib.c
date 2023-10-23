#import bytereader
#import time

// http://midi.teragonaudio.com/
// https://www.recordingblogs.com
// https://www.recordingblogs.com/wiki/time-division-of-a-midi-file
// https://ibex.tech/resources/geek-area/communications/midi/midi-comms
// http://www.music.mcgill.ca/~ich/classes/mumt306/StandardMIDIfileformat.html

pub typedef {
    bytereader.reader_t *r;

	int delta_time_unit;
	int delta_time_value;

	/*
	 * The MIDI spec allows to omit the status byte (event type)
	 * if the previous transmitted message had the same status.
	 * This is referred to as running status.
	 */
	int running_status;

	/*
	 * Current time position.
	 */
	time.duration_t time;

	int quarter_note_duration;
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
	m->quarter_note_duration = 500000; // us

    chunk_head_t h = {};
    if (!midibin_read_chunk_head(m, &h)) {
        printf("failed to read head: %s\n", strerror(errno));
		bytereader.freereader(r);
		free(m);
        return NULL;
    }
    if (strcmp("MThd", h.name)) {
		printf("not a midi file: expected MThd, got %s\n", h.name);
        bytereader.freereader(r);
		free(m);
        return NULL;
    }
    if (h.length != 6) {
        printf("expected length 6, got %u\n", h.length);
		bytereader.freereader(r);
		free(m);
        return NULL;
    }

    uint16_t format = bytereader.read16(m->r);
    uint16_t ntracks = bytereader.read16(m->r);

    // Delta time unit. Either ticks per beat or frames per second.
    uint16_t division = (int) bytereader.read16(m->r);
    if (division > 0) {
		// If the value is positive, then it's in ticks per beat.
    	// For example, +96 would mean 96 ticks per beat.
		// How many ticks is one "beat" (quarter note).
		m->delta_time_unit = TICKS_PER_BEAT;
		m->delta_time_value = division;
    } else {
		// If the value is negative, delta times are in SMPTE compatible units.
		// the following 15 bits (bit mask 0x7FFF) describe the time division in frames per second.
		m->delta_time_unit = FRAMES_PER_SECOND;
		m->delta_time_value = -division;
    }

    printf("header chunk: bytes: %u, format: %s, tracks: %u, ticks per beat: %d\n",
        h.length,
        formatname(format),
        ntracks,
        division
    );
    return m;
}

pub void read_file(midi_t *m) {
	while (true) {
		// track_chunk = "MTrk" + <length> + <track_event> [+ <track_event> ...]
		chunk_head_t h = {};
		if (!midibin_read_chunk_head(m, &h)) {
			panic("failed to read head: %s\n", strerror(errno));
		}
		time.dur_set(&m->time, 0, time.US);
		if (strcmp("MTrk", h.name)) {
			panic("expected MTrk, got %s", h.name);
		}
		printf("track chunk length: %d\n", h.length);
		bool more = read_track_chunk_rest(m);
		if (!more) {
			break;
		}
    }
}

bool read_track_chunk_rest(midi_t *m) {
	while (!bytereader.ended(m->r)) {
        bool more = read_track_event(m);
		if (!more) {
			break;
		}
    }
    return true;
}

bool read_track_event(midi_t *m) {
	// Elapsed time (delta time) from the previous event to this event.
	int delta_ticks = midibin_variable_length_value(m->r);
	double delta_quarters = delta_ticks / m->delta_time_value;
	double delta_us = delta_quarters * m->quarter_note_duration;
	time.dur_add(&m->time, delta_us, time.US);
	char formatted_time[100] = {0};
	if (!time.dur_fmt(&m->time, formatted_time, sizeof(formatted_time))) {
		panic("failed to format time");
	}
	if (delta_ticks == 0) {
		printf("%30s\t", ".");
	} else {
		printf("%s (+%d ticks=%.1fq)\t", formatted_time, delta_ticks, delta_quarters);
	}

	if (m->running_status == 0 || get_status_type(bytereader.peekc(m->r)) != STATUS_UNKNOWN) {
		m->running_status = bytereader.readc(m->r);
	}

	int event_type = m->running_status;

	switch (get_status_type(m->running_status)) {
		case STATUS_META: {
			bool ended = read_meta_message(m);
			return !ended;
		}
		case STATUS_CHANNEL: {
			read_channel_message(m);
			return true;
		}
	}

	// The messages from 0xF0 to 0xFF are called System Messages; they do not affect any particular channel.
	// <sysex_event>
	//     an SMF system exclusive event.
	// System Exclusive Event
	// A system exclusive event can take one of two forms:
	// sysex_event = 0xF0 + <data_bytes> 0xF7 or sysex_event = 0xF7 + <data_bytes> 0xF7
	// In the first case, the resultant MIDI data stream would include the 0xF0. In the second case the 0xF0 is omitted.

	printf("(?) message 0x%X\n", event_type);
	// panic("dunno");
	return true;
}

bool read_meta_message(midi_t *m) {
	int meta_type = bytereader.readc(m->r);
	int v_length = midibin_variable_length_value(m->r);
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
			// Tempo in microseconds per quarter-note.
			// If no set tempo event is present, 120 beats per minute is assumed.
			// MICROSECONDS_PER_MINUTE = 60000000
			// BPM = MICROSECONDS_PER_MINUTE / MPQN
			// MPQN = MICROSECONDS_PER_MINUTE / BPM
			printf("Tempo: %u us per quarter note\n", data_as_number);
			m->quarter_note_duration = data_as_number;
		}
		case META_SEQ_NAME: {
			printf("Track name: \"%s\"\n", data);
		}
		case META_END_OF_TRACK: {
			printf("---------- end of track -----------\n");
		}
		// At least one Time Signature Event should appear in the first track chunk (or all track chunks in a Type 2 file) before any non-zero delta time events. If one is not specified 4/4, 24, 8 should be assumed.
		case META_TIME_SIGNATURE: {
			// Time signature is <numerator> / 2^<power>.
			// For "4/4" numerator = 4, power = 2.
			int numerator = idata[0];
			int power = idata[1];

			// How many clock signals there are in one metronome click.
			// Clock signals come at a rate of 24 signals per quarter-note.
			// 24 = click once every quarter-note, 48 = click once every half-note.
			int ticks_per_click = idata[2];

			// the number of 32nd notes per 24 MIDI clock signals. This value is usually 8 because there are usually 8 32nd notes in a quarter-note.
			int n32th_per_beat = idata[3];
			printf("Time signature: %d/2^%d, metronome clicks once every %d midi clocks, %d 32th notes per beat\n", numerator, power, ticks_per_click, n32th_per_beat);
		}
		default: {
			printf("META\t%s \t(%d bytes)\t\"%s\"\n", meta_name(meta_type), v_length, data);
		}
	}
	return meta_type == META_END_OF_TRACK;
}

void read_channel_message(midi_t *m) {
	int event_type = m->running_status;
	// The second four bits specify which channel the message affects.
	int type = (event_type >> 4) & 0xF;
	int channel = event_type & 0xF;

	// printf("type = %x (%s), channel = %x\n", type, cmdname(type), channel);
	switch (type) {
		case 0x8: {
			// 0x80-0x8F 	Note off 	2 (note, velocity)
			int key = bytereader.readc(m->r); // which key to release
			int velocity = bytereader.readc(m->r); // how fast/hard the key was released (0..127)
			printf("\tnote off\tchannel=%d\tnote=%d\tvelocity=%d\n", channel, key, velocity);
		}
		case 0x9: {
			// 0x90-0x9F - play a note on channel (0..F)
			int key = bytereader.readc(m->r); // which of the 128 MIDI keys is being played (0..127)
			int velocity = bytereader.readc(m->r); // volume and intensity (0..127)
			printf("\tnote on \tchannel=%d\tnote=%d\tvelocity=%d\n", channel, key, velocity);
		}
		case 0xA: {
			// 0xA0-0xAF 	Key Pressure 	2 (note, key pressure)
			int arg1 = bytereader.readc(m->r);
			int arg2 = bytereader.readc(m->r);
			printf("\tkey pressure, channel=%d note,pressure=%d,%d\n", channel, arg1, arg2);
		}
		case 0xB: {
			// 0xB0-0xBF 	Control Change 	2 (controller no., value)
			// change in a MIDI channels state (knob setting)
			// There are 128 controllers which define different attributes of the channel including volume, pan, modulation, effects, and more.
			int controller = bytereader.readc(m->r);
			int value = bytereader.readc(m->r);
			printf("0x%X 0x%X 0x%X ch %d: set %-50s %d\n", event_type, controller, value, channel, controller_name(controller), value);
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

int midibin_variable_length_value(bytereader.reader_t *r) {
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


// --------------- tables -------------------

pub enum {
	TICKS_PER_BEAT,
	FRAMES_PER_SECOND
};

/*
 * File contents type:
 * 0 = single multi-channel track
 * 1 = one or more simultaneous tracks (or MIDI outputs) of a sequence
 * 2 = one or more sequentially independent single-track patterns
 */
enum {
    FORMAT_ONE_MULTI_CHANNEL_TRACK = 0,
    FORMAT_MANY_SIMULTANEOUS_TRACKS = 1,
    FORMAT_MANY_INDEPENDENT_TRACKS = 2
};

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

const char *formatname(int format) {
    switch (format) {
        case FORMAT_ONE_MULTI_CHANNEL_TRACK: { return "0, single track"; }
        case FORMAT_MANY_SIMULTANEOUS_TRACKS: { return "1, multiple tracks"; }
        case FORMAT_MANY_INDEPENDENT_TRACKS: { return "2, multiple songs"; }
        default: { return "unknown format"; }
    }
}


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