#import reader
#import enc/endian
#import writer

pub typedef {
    uint16_t format; // 1=PCM
    uint16_t channels; // 1 or 2
    uint32_t frequency; // 44100 (Hz)
    uint16_t bits_per_sample;
} wav_t;

pub typedef {
	reader.t *reader;
	FILE *file; // hack

	wav_t wav;
	uint32_t done, datalen;
} reader_t;

pub typedef {
	writer.t *writer;
} writer_t;

pub typedef {
	int left, right;
} sample_t;

// Starts a wave stream writing to file f.
pub writer_t *open_writer(FILE *f) {
	writer.t *wr = writer.file(f);
	writer_t *w = calloc!(1, sizeof(writer_t));
	w->writer = wr;
	write_headers(w);
	return w;
}

pub void close_writer(writer_t *w) {
	writer.free(w->writer);
	OS.free(w);
}

pub reader_t *open_reader(const char *path) {
	uint32_t tmp4u;
    uint16_t tmp2u;

    FILE *f = fopen(path, "rb");
    reader.t *r = reader.file(f);

    //
    // RIFF header
    //
    if (!readtag(r, "RIFF")) {
		fclose(f);
		reader.free(r);
		return false;
	}
    endian.read4le(r, &tmp4u); // riff chunk length

    //
    // Wave header
    //
    if (!readtag(r, "WAVE") || !readtag(r, "fmt ")) {
		fclose(f);
		reader.free(r);
		return false;
	}
    endian.read4le(r, &tmp4u); // fmt chunk size, 16 bytes
    wav_t w = {};
    endian.read2le(r, &w.format);
    endian.read2le(r, &w.channels);
    endian.read4le(r, &w.frequency);
    endian.read4le(r, &tmp4u); // bytes per second, same as freq * bytes per block (176400)
    endian.read2le(r, &tmp2u); // bytes per block, same as channels * bytes per sample (4)
    endian.read2le(r, &w.bits_per_sample);
    
    if (w.format != 1) panic("expected format 1, got %u", w.format);
    if (w.channels != 2) panic("expected 2 channels, got %u", w.channels);

    //
    // INFO block
    //
    if (!readinfo(r)) {
		fclose(f);
		reader.free(r);
		return false;
	}

	reader_t *wr = calloc!(1, sizeof(reader_t));
	wr->reader = r;
	wr->wav = w;
	wr->file = f;

    //
    // PCM data
    //
    if (!readtag(r, "data")) {
		fclose(f);
		reader.free(r);
		OS.free(wr);
		return false;
	}
    endian.read4le(r, &wr->datalen);
	return wr;
}

pub bool more(reader_t *r) {
	return r->done < r->datalen;
}

pub sample_t read_sample(reader_t *r) {
	sample_t sa = {};

	uint16_t u = 0;
	endian.read2le(r->reader, &u);
	int s = (int) u;
	if (s >= 32768) {
		s -= 65536;
	}
	sa.left = s;

	endian.read2le(r->reader, &u);
	s = (int) u;
	if (s >= 32768) {
		s -= 65536;
	}
	sa.right = s;

	int bps = r->wav.bits_per_sample / 8;
	r->done += bps;
    return sa;
}

void write_headers(writer_t *w) {
	uint32_t riff_length = -1;
	uint32_t datalen = -1;
	writetag(w->writer, "RIFF");
    endian.write4le(w->writer, riff_length);
    writetag(w->writer, "WAVE");
	writetag(w->writer, "fmt ");

    endian.write4le(w->writer, 16); // fmt chunk size, 16 bytes
	uint16_t channels = 2;
	uint16_t bytes_per_sample = 2;
	uint16_t bytes_per_block = channels * bytes_per_sample;
    endian.write2le(w->writer, 1); // format
    endian.write2le(w->writer, 2); // channels
    endian.write4le(w->writer, 44100); // frequency
    endian.write4le(w->writer, 44100 * bytes_per_block); // bytes per second
    endian.write2le(w->writer, bytes_per_block);
    endian.write2le(w->writer, bytes_per_sample * 8); // bits per sample
	writetag(w->writer, "data");
    endian.write4le(w->writer, datalen);
}

pub void write_sample(writer_t *w, int left, right) {
	int s = left;
	if (s < 0) s += 65536;
	endian.write2le(w->writer, (uint16_t) s);

	s = right;
	if (s < 0) s += 65536;
	endian.write2le(w->writer, (uint16_t) s);
}

pub void close_reader(reader_t *r) {
    reader.free(r->reader);
    fclose(r->file);
	OS.free(r);
}

bool readinfo(reader.t *r) {
	char tmp[1000];
    if (!readtag(r, "LIST")) return false;
    uint32_t listlen = 0;
    endian.read4le(r, &listlen);
    if (!readtag(r, "INFO")) return false;
    listlen -= 4;
    while (listlen > 0) {
        char key[5] = {};
        reader.read(r, (uint8_t*)key, 4);
        uint32_t infolen;
        endian.read4le(r, &infolen);
        listlen -= 8;

        reader.read(r, (uint8_t*)tmp, infolen);
        tmp[infolen] = '\0';
        printf("# %s = %s\n", key, tmp);
        listlen -= infolen;
    }
    return true;
}

bool readtag(reader.t *r, const char *tag) {
    char tmp[5] = {};
    reader.read(r, (uint8_t*) tmp, 4);
    if (strcmp(tmp, tag)) panic("wanted %s, got %s", tag, tmp);
    return true;
}

void writetag(writer.t *w, const char *tag) {
	writer.write(w, (uint8_t *)tag, 4);
}
