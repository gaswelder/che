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

pub typedef { double left, right; } samplef_t;

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
    FILE *f = fopen(path, "rb");
	if (!f) {
		return NULL;
	}
    reader.t *r = reader.file(f);

	reader_t *wr = calloc!(1, sizeof(reader_t));
	wr->reader = r;

	wav_t w = {};
    if (!read_headers(r, &w, &wr->datalen)) {
		fclose(f);
		reader.free(r);
	}
	wr->wav = w;
	wr->file = f;

	return wr;
}

bool read_headers(reader.t *r, wav_t *wp, uint32_t *datalen) {
	//
    // Begin RIFF
    //
    if (!expect_tag(r, "RIFF")) return false;
	uint32_t tmp4u;
    endian.read4le(r, &tmp4u); // riff chunk length

    //
    // Begin WAVE
    //
    if (!expect_tag(r, "WAVE")) return false;

	//
	// wav fmt struct
	//
	if (!expect_tag(r, "fmt ")) return false;
    endian.read4le(r, &tmp4u); // fmt chunk size, 16 bytes
	if (tmp4u != 16) panic("expected 16 bytes fmt chunk, got %u", tmp4u);
	uint8_t buf[16] = {};
	reader.read(r, buf, 16);
	wav_t w = read_fmt(buf);

    if (w.format != 1) {
		panic("expected format 1 (PCM), got %u", w.format);
	}
    if (w.channels != 2 && w.channels != 1) {
		panic("expected 1 or 2 channels, got %u", w.channels);
	}
	if (w.bits_per_sample != 16 && w.bits_per_sample != 24) {
		panic("expected 16 or 24 bits per sample, got %d", w.bits_per_sample);
	}
	*wp = w;

	//
    // Optional INFO block
    //
	char tag[5] = {};
    reader.read(r, (uint8_t*) tag, 4);
	if (strcmp(tag, "LIST") == 0) {
		if (!readinfo(r)) {
			return false;
		}
		reader.read(r, (uint8_t*) tag, 4);
	}

	//
	// Optional padding
	//
	if (strcmp(tag, "PAD ") == 0) {
		endian.read4le(r, &tmp4u);
		uint8_t buf[4096] = {};
		while (tmp4u > 0) {
			size_t n = tmp4u;
			if (n > 4096) n = 4096;
			reader.read(r, buf, n);
			tmp4u -= n;
		}
		reader.read(r, (uint8_t*) tag, 4);
	}

	//
    // PCM data
    //
	if (strcmp(tag, "data") != 0) {
		panic("expected \"data\" chunk, got \"%s\"", tag);
	}
    endian.read4le(r, datalen);
	return true;
}

void write_headers(writer_t *w) {
	//
	// Start RIFF
	//
	uint32_t riff_length = -1;
	writetag(w->writer, "RIFF");
	endian.write4le(w->writer, riff_length);

	//
	// Start WAVE
	//
    writetag(w->writer, "WAVE");

	//
	// Wave fmt struct
	//
	writetag(w->writer, "fmt ");
	endian.write4le(w->writer, 16); // fmt chunk size, 16 bytes
	wav_t fmt = {
		.format = 1,
		.channels = 2,
		.frequency = 44100,
		.bits_per_sample = 16
	};
	write_fmt(w->writer, fmt);

	//
	// Start an infinite data chunk
	//
	writetag(w->writer, "data");
	uint32_t datalen = -1;
    endian.write4le(w->writer, datalen);
}

wav_t read_fmt(uint8_t *buf) {
	uint32_t tmp4u;
    uint16_t tmp2u;
	reader.t *r = reader.static_buffer(buf, 16);
	wav_t w = {};
    endian.read2le(r, &w.format);
    endian.read2le(r, &w.channels);
    endian.read4le(r, &w.frequency);
    endian.read4le(r, &tmp4u); // bytes per second, same as freq * bytes per block (176400)
    endian.read2le(r, &tmp2u); // bytes per block, same as channels * bytes per sample (4)
    endian.read2le(r, &w.bits_per_sample);
	reader.free(r);
	return w;
}

void write_fmt(writer.t *bw, wav_t fmt) {
	uint16_t channels = fmt.channels;
	uint16_t bytes_per_sample = fmt.bits_per_sample / 8;
	uint16_t bytes_per_block = channels * bytes_per_sample;
    endian.write2le(bw, fmt.format);
    endian.write2le(bw, fmt.channels);
    endian.write4le(bw, fmt.frequency);
    endian.write4le(bw, fmt.frequency * bytes_per_block); // bytes per second
    endian.write2le(bw, bytes_per_block);
    endian.write2le(bw, fmt.bits_per_sample); // bits per sample
}

pub bool more(reader_t *r) {
	return r->done < r->datalen;
}

int read_sample0(reader_t *r) {
	int bps = r->wav.bits_per_sample / 8;
	switch (r->wav.bits_per_sample) {
		// 8 bit - [0, 255], zero at 128
		// 16-bit - [-32768, +32767], zero at 0
		case 16: {
			uint16_t u = 0;
			endian.read2le(r->reader, &u);
			r->done += bps;
			int s = (int) u;
			if (s >= 32768) s -= 65536;
			return s;
		}
		// 24-bit - [−8,388,608, +8,388,607], zero at 0.
		case 24: {
			uint32_t u = 0;
			endian.read3le(r->reader, &u);
			r->done += bps;
			int s = (int) u;
			if (s >= 8388608) s -= 2 * 8388608;
			return s;
		}
		default: {
			panic("unimplemented sample size: %d", r->wav.bits_per_sample);
		}
	}
	return 0;
}

pub samplef_t read_samplef(reader_t *r) {
	sample_t s = read_sample(r);
	double scale = 1 << (r->wav.bits_per_sample-1);
	samplef_t sa = {
		.left = (double) s.left / scale,
		.right = (double) s.right / scale
	};
	return sa;
}

pub sample_t read_sample(reader_t *r) {
	sample_t sa = {};
	if (r->wav.channels == 2) {
		sa.left = read_sample0(r);
		sa.right = read_sample0(r);
		return sa;
	}
	if (r->wav.channels == 1) {
		sa.left = read_sample0(r);
		sa.right = sa.left;
		return sa;
	}
	panic("unimplemented channels number: %d", r->wav.channels);
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
    uint32_t listlen = 0;
    endian.read4le(r, &listlen);
    if (!expect_tag(r, "INFO")) return false;
    listlen -= 4;
    while (listlen > 0) {
        char key[5] = {};
        reader.read(r, (uint8_t*)key, 4);
        uint32_t infolen;
        endian.read4le(r, &infolen);
        listlen -= 8;

        reader.read(r, (uint8_t*)tmp, infolen);
        tmp[infolen] = '\0';
        // printf("# %s = %s\n", key, tmp);
        listlen -= infolen;
    }
    return true;
}

bool expect_tag(reader.t *r, const char *tag) {
    char tmp[5] = {};
    reader.read(r, (uint8_t*) tmp, 4);
    if (strcmp(tmp, tag)) panic("wanted %s, got %s", tag, tmp);
    return true;
}

void writetag(writer.t *w, const char *tag) {
	writer.write(w, (uint8_t *)tag, 4);
}
