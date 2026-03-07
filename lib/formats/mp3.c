#import bits
#import enc/endian
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
	size_t time; // Current time position in microseconds.
	header_t h; // Currently loaded frame header.
	size_t framelen; // Length of the loaded frame in bytes, including the header.
	uint8_t frame[1100]; // The loaded frame with the header.	
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

	// Sync: find the first frame.
	while (fpeek(m->file) != EOF) {
		if (readframe(m)) {
			break;
		}
	}
	// Skip the first frame if it's an info frame.
	if (info_frame(m)) {
		readframe(m);
	}

	if (feof(m->file)) {
		seterr(err, "format not recognized");
		OS.free(m);
		return NULL;
	}
	return m;
}

bool info_frame(reader_t *r) {
	// Info frame is:
	// 32: normal header
	// xx: normal sideinfo
	// ??: xing metadata
	// ??: padding

	int sideinfo_size = 32;
	if (r->h.mode == 3) {
		sideinfo_size = 17;
	}

	uint8_t *p = r->frame + 4 + sideinfo_size;
	reader.t *br = reader.static_buffer(p, 123123);

	xing_header_t xing = {};
	bool ok = read_xing(br, &xing);
	if (ok) {
		printf("Xing header found\n");
		printf("Flags: 0x%08x\n", xing.flags);
		printf("Frames: %u\n", xing.frames);
		printf("Bytes: %u\n", xing.bytes);
		printf("Quality: %u\n", xing.quality);
	}
	reader.free(br);
	return ok;
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
	if (!readframe(f)) {
		if (!feof(f->file)) {
			panic("stream error");
		}
		return false;
	}
	return true;
}

// Reads a single frame.
// Each frame is a complete, independent unit, with its owh header and data.
bool readframe(reader_t *f) {
	// Read 4 bytes and parse the header.
	for (int i = 0; i < 4; i++) {
		int c = fgetc(f->file);
		if (c == EOF) {
			return false;
		}
		f->frame[i] = (uint8_t) c;
	}
	header_t *h = &f->h;
	reader.t *fr = reader.static_buffer(f->frame, 4);
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
	// The encoder inserts padding bits from time to time to compensate for
	// the truncation from integer division, somewhat similar to leap year
	// seconds.
	size_t len = 144 * (h->bitrate*1000) / 44100;
	if (h->padded) {
		len++;
	}

	// Read the rest of the frame.
	for (size_t i = 4; i < len; i++) {
		int c = fgetc(f->file);
		if (c == EOF) {
			return false;
		}
		f->frame[i] = (uint8_t) c;
	}
	f->framelen = len;
	f->time += frame_samples * 1000000 / h->freq;
	return true;
}

bool readheader(bits.reader_t *s, header_t *h) {
	// Frame sync: FF, F
	if (bits.readn(s, 8) != 0xFF) {
		return false;
	}
	if (bits.readn(s, 4) != 0xF) {
		return false;
	}

	// 8: FF
	// 4: F
	// 1: version, 1 for MPEG1
	// 2: layer, 1 for MP3
	// 1: CRC present
	// 4: bitrate
	// 2: frequency
	// 1: padding present
	// 1: private
	// 2: mode
	// 2: ext
	// 1: copyright?
	// 1: original?
	// 2: emphasis

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
	if (index < 0 || (size_t)index >= nelem(frequencies)) {
		return false;
	}
	h->freq = frequencies[index];
	if (h->freq != 44100) {
		panic("Unsupported sampling frequency: %d", h->freq);
	}

	int tmp = bits.readn(s, 1); // 1 if padding present
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

// Writes current frame to the given file.
pub void write_frame(reader_t *f, FILE *out) {
	for (size_t i = 0; i < f->framelen; i++) {
		fputc(f->frame[i], out);
	}
}

int fpeek(FILE *f) {
	int c = fgetc(f);
	if(c == EOF) return EOF;
	assert(ungetc(c, f) == c);
	return c;
}

// The first of a VBR file may contain info instead of audio data.
// Among that info could be this xing header.
typedef {
    uint32_t flags; // which of the fields below are set.
    uint32_t frames; // audio length in frames.
    uint32_t bytes; // audio length in bytes.
    uint8_t toc[100]; // toc[i] is approximate file position for i% audio position.
    uint32_t quality; // LAME metadata.
} xing_header_t;

bool read_xing(reader.t *r, xing_header_t *x) {
    uint8_t magic[4];
    reader.read(r, magic, 4);

    if (memcmp(magic, "Xing", 4) != 0 && memcmp(magic, "Info", 4) != 0) {
        return false;
    }

    endian.read4be(r, &x->flags);
    if (x->flags & 0x1) {
        endian.read4be(r, &x->frames);
    }
    if (x->flags & 0x2) {
        endian.read4be(r, &x->bytes);
    }
    if (x->flags & 0x4) {
        reader.read(r, x->toc, 100);
    }
    if (x->flags & 0x8) {
        endian.read4be(r, &x->quality);
    }
    return true;
}