#import bits
#import reader

/*
 * This recognizes only MPEG 1 Layer III.
 * http://mpgedit.org/mpgedit/mpeg_format/MP3Format.html
 * http://mpgedit.org/mpgedit/mpeg_format/mpeghdr.htm
 */

// Bitrates table for MP3, kbps.
int bitrates[] = {
	0, 32, 40, 48, 56, 64, 80, 96, 112,
	128, 160, 192, 224, 256, 320
};

// Sampling frequencies table for MP3, Hz.
int frequencies[] = {44100, 48000, 32000};

// Number of samples in a frame.
// For mp3 it's always 1152.
const int frame_samples = 1152;


pub typedef {
	int bitrate;
	int freq;
	int mode;
	bool padded;
} header_t;

pub typedef {
	FILE *file; // The file being read from.
	header_t h; // Currently loaded frame header.
	size_t time; // Current time position in microseconds.

	/*
	 * Current position in frames, assuming 44100 Hz
	 * and 1152 samples per frame.
	 */
	size_t framepos;
	int nextpos;
} reader_t;

pub typedef {
	bool set;
	char msg[100];
} err_t;

void seterr(err_t *err, const char *fmt, ...) {
	va_list args = {};
	va_start(args, fmt);
	vsnprintf(err->msg, sizeof(err->msg), fmt, args);
	va_end(args);
	err->set = true;
}

// Creates a reader for mp3 file at given path.
pub reader_t *open_reader(const char *path, err_t *err) {
	FILE *f = fopen(path, "rb");
	if (!f) {
		seterr(err, "failed to open '%s': %s", path, strerror(errno));
		return NULL;
	}

	reader_t *m = calloc!(1, sizeof(reader_t));
	m->file = f;

	/*
	 * Sync: find first valid header
	 */
	while(fpeek(m->file) != EOF) {
		if(readframe(m)) {
			break;
		}
	}
	if (feof(m->file)) {
		seterr(err, "format not recognized");
		OS.free(m);
		return NULL;
	}
	return m;
}

// Returns bitrate of the reader's current frame, in kbps.
pub int bitrate(reader_t *r) {
	return r->h.bitrate;
}

// Closes the reader.
pub void close_reader(reader_t *f) {
	fclose(f->file);
	free(f);
}

// Reads next frame. Returns false on error.
pub bool nextframe(reader_t *f) {
	if(fseek(f->file, f->nextpos, SEEK_SET) < 0) {
		panic("fseek failed");
	}

	if(!readframe(f)) {
		if(!feof(f->file)) {
			panic("stream error");
		}
		return false;
	}

	f->framepos++;
	return true;
}

// Reads a single frame.
// Each frame is a complete, independent unit, with its owh header and data.
bool readframe(reader_t *f) {
	header_t *h = &f->h;

	// Read the header (32 bits).
	reader.t *fr = reader.file(f->file);
	bits.reader_t *s = bits.newreader(fr);
	bool r = readheader(s, h);
	bits.closereader(s);
	reader.free(fr);
	if (!r) {
		return false;
	}

	// Calculate the length of the frame.
	// 144 is 1152/8 - number of bytes required for 1152 samples.
	// Note that this is integer division that truncates the remainder.
	size_t len = 144 * (h->bitrate*1000) / h->freq;

	// The encoder inserts padding bits from time to time to compensate for
	// the truncation from integer division, somewhat similar to leap year
	// seconds.
	if (h->padded) len++;

	// Instead of reading data just remember the next frame position.
	f->nextpos = ftell(f->file) + (int)len - 4;

	f->time += frame_samples * 1000000 / 44100;
	return r;
}

bool readheader(bits.reader_t *s, header_t *h) {
	// Frame sync: FF, F
	if (bits.readn(s, 8) != 0xFF) {
		return false;
	}
	if (bits.readn(s, 4) != 0xF) {
		return false;
	}

	// 1 bit: version, '1' for MPEG1
	if (bits.readn(s, 1) != 1) {
		return false;
	}
	// 2 bits: "layer", 1 for MP3
	if (bits.readn(s, 2) != 1) {
		return false;
	}
	// 1 bit: error protection
	if (bits.readn(s, 1) == 0) {
		// 16-bit CRC will be somewhere
		panic("+CRC");
	}

	// 4 bits: bitrate
	int index = bits.readn(s, 4);
	h->bitrate = bitrates[index];

	// 2: freq
	index = bits.readn(s, 2);
	if(index < 0 || (size_t)index >= nelem(frequencies)) {
		return false;
	}
	h->freq = frequencies[index];
	if (h->freq != 44100) {
		panic("Unsupported sampling frequency: %d", h->freq);
	}

	int tmp = bits.readn(s, 1); // 1: padding?
	if (tmp < 0) panic("!");
	h->padded = tmp == 1;

	bits.readn(s, 1); // 1: private
	h->mode = bits.readn(s, 2); // 2: mode
	bits.readn(s, 2); // 2: ext
	bits.readn(s, 1); // 1: copyright?
	bits.readn(s, 1); // 1: original?
	bits.readn(s, 2); // 2: emphasis

	// (32 bits total)
	return true;
}

// Writes out the data from f to out starting at current position in f
// until the position pos.
pub void writeout(reader_t *f, FILE *out, size_t usec) {
	// Calculate how many frames are fully below the specified position:
	// nframes * samples_per_frame / 44100 <= pos.
	// Next frame must not go over the specified position.
	while (f->framepos * frame_samples * 1000000 <= 44100 * usec) {
		write_frame(f, out);
		if (!nextframe(f)) {
			break;
		}
	}
}

/*
 * Writes current frame to the given file.
 */
void write_frame(reader_t *f, FILE *out) {
	/*
	 * Go back 4 bytes to the header
	 */
	if(fseek(f->file, -4, SEEK_CUR) < 0) {
		panic("fseek(-4) failed");
	}

	size_t len = f->nextpos - ftell(f->file);
	while(len > 0) {
		len--;
		int c = fgetc(f->file);
		fputc(c, out);
	}
}

int fpeek(FILE *f) {
	int c = fgetc(f);
	if(c == EOF) return EOF;
	assert(ungetc(c, f) == c);
	return c;
}
