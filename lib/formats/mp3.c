/*
 * This recognizes only MPEG 1 Layer III.
 */

/*
 * http://mpgedit.org/mpgedit/mpeg_format/MP3Format.html
 * http://mpgedit.org/mpgedit/mpeg_format/mpeghdr.htm
 */

#import bitreader

const int L3 = 1;

int bitrates[] = {
	0, 32, 40, 48, 56, 64, 80, 96, 112,
	128, 160, 192, 224, 256, 320
};
int frequencies[] = {44100, 48000, 32000};

/*
 * Number of samples in a frame
 */
const int frame_samples = 1152;


pub typedef {
	int bitrate;
	int freq;
	int mode;
	bool padded;
} header_t;

pub typedef {
	FILE *file;
	const char *err;

	/*
	 * Current position in frames, assuming 44100 Hz
	 * and 1152 samples per frame.
	 */
	size_t framepos;

	int nextpos;

	header_t h;
} mp3file;

pub typedef {
	int min;
	int sec;
	int usec;
} mp3time_t;

/*
 * Opens an MP3 file and finds the beginning of the stream.
 */
pub mp3file *mp3open(const char *path) {
	FILE *f = fopen(path, "rb");
	if (!f) {
		return NULL;
	}

	mp3file *m = calloc!(1, sizeof(mp3file));
	m->file = f;

	/*
	 * Sync: find first valid header
	 */
	while(fpeek(m->file) != EOF) {
		if(read_header(m)) {
			break;
		}
	}
	if(feof(m->file)) {
		m->err = "Format not recognized";
	}

	return m;
}

/*
 * Closes the MP3 file.
 */
pub void mp3close(mp3file *f)
{
	fclose(f->file);
	free(f);
}

/*
 * Returns last error string for the file.
 */
pub const char *mp3err(mp3file *f)
{
	return f->err;
}

/*
 * Reads next frame. Returns false on error.
 */
bool nextframe(mp3file *f)
{
	if(f->err) {
		panic("can't nextframe: %s", f->err);
	}

	if(fseek(f->file, f->nextpos, SEEK_SET) < 0) {
		panic("fseek failed");
	}

	if(!read_header(f)) {
		if(!feof(f->file) && !f->err) {
			f->err = "Stream error";
		}
		return false;
	}

	f->framepos++;
	return true;
}

/*
 * Writes data from 'f' to 'out' until the specified time position.
 */
pub void mp3out(mp3file *f, FILE *out, mp3time_t pos)
{
	if(f->err) return;
	/*
	 * How many frames are completely below the specified
	 * time:
	 * nframes * samples_per_frame / 44100 <= time_sec
	 */
	size_t usec = 0;
	if (pos.min == -1) {
		usec = SIZE_MAX;
	} else {
		usec = pos.usec + 1000000 * (pos.sec + 60*pos.min);
	}

	/*
	 * Next frame must not go over the specified position.
	 */
	while(f->framepos * frame_samples * 1000000 <= 44100 * usec) {
		/*
		 * Write the current frame and go to the next one.
		 */
		write_frame(f, out);
		if(!nextframe(f)) {
			break;
		}
	}
}

/*
 * Writes current frame to the given file.
 */
pub void write_frame(mp3file *f, FILE *out)
{
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


bool read_header(mp3file *f)
{
	bitreader.bits_t *s = bitreader.bits_new(f->file);
	bool r = _read_header(s, &f->h);
	bitreader.bits_free(s);

	if (!r) {
		return false;
	}

	/*
	 * Calculate the length of the frame.
	 */
	header_t *h = &f->h;
	size_t len = 144 * (h->bitrate*1000) / h->freq;
	if(h->padded) len++;

	/*
	 * Remember next frame position.
	 */
	f->nextpos = ftell(f->file) + (int)len - 4;

	return r;
}

bool _read_header(bitreader.bits_t *s, header_t *h)
{
	// 8 bits: FF
	if(bitreader.bits_getn(s, 8) != 0xFF) {
		return false;
	}

	// 4 bits: F
	if(bitreader.bits_getn(s, 4) != 0xF) {
		return false;
	}

	// 1 bit: version
	// '1' for MPEG1
	if(bitreader.bits_getn(s, 1) != 1) {
		return false;
	}

	// 2 bits: layer
	if(bitreader.bits_getn(s, 2) != L3) {
		return false;
	}

	// 1 bit: error protection
	if(bitreader.bits_getn(s, 1) == 0) {
		// 16-bit CRC will be somewhere
		panic("+CRC");
	}

	// 4 bits: bitrate
	int index = bitreader.bits_getn(s, 4);
	h->bitrate = bitrates[index];

	// 2: freq
	index = bitreader.bits_getn(s, 2);
	if(index < 0 || (size_t)index >= nelem(frequencies)) {
		return false;
	}
	h->freq = frequencies[index];
	/*
	 * To make first draft simpler, stick only to
	 * the regular 44100 Hz format.
	 */
	if(h->freq != 44100) {
		panic("Unsupported sampling frequency: %d", h->freq);
	}
	/*
	 * General support will have to assume variable frequency
	 * (since there are such files) and some tricks in calculating
	 * current position in some normalized time units.
	 */

	// 1: padding?
	h->padded = (bool) bitreader.bits_getn(s, 1);

	// 1: private
	bitreader.bits_getn(s, 1);

	// 2: mode
	h->mode = bitreader.bits_getn(s, 2);

	// 2: ext
	bitreader.bits_getn(s, 2);
	// 1: copyright?
	bitreader.bits_getn(s, 1);
	// 1: original?
	bitreader.bits_getn(s, 1);
	// 2: emphasis
	bitreader.bits_getn(s, 2);

	return true;
}

int fpeek(FILE *f) {
	int c = fgetc(f);
	if(c == EOF) return EOF;
	assert(ungetc(c, f) == c);
	return c;
}
