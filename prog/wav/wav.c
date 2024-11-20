#import reader
#import enc/endian

char tmp[1000] = {};

typedef {
    uint16_t format; // 1=PCM
    uint16_t channels; // 1 or 2
    uint32_t frequency; // 44100 (Hz)
    uint16_t bits_per_sample;
} wav_t;

int main() {
    if (!readwav()) {
        return 1;
    }
    return 0;
}

bool readwav() {
    uint32_t tmp4u;
    uint16_t tmp2u;

    FILE *f = fopen("out.wav", "rb");
    reader.t *r = reader.file(f);

    //
    // RIFF header
    //
    if (!readtag(r, "RIFF")) return false;
    endian.read4le(r, &tmp4u); // riff chunk length

    //
    // Wave header
    //
    if (!readtag(r, "WAVE")) return false;
    if (!readtag(r, "fmt ")) return false;
    endian.read4le(r, &tmp4u); // fmt chunk size, 16 bytes
    wav_t w = {};
    endian.read2le(r, &w.format);
    endian.read2le(r, &w.channels);
    endian.read4le(r, &w.frequency);
    endian.read4le(r, &tmp4u); // bytes per second, same as freq * bytes per block (176400)
    endian.read2le(r, &tmp2u); // bytes per block, same as channels * bytes per sample (4)
    endian.read2le(r, &w.bits_per_sample);

    printf("format = %u\n", w.format);
    printf("channels = %u\n", w.channels);
    printf("freq = %u Hz\n", w.frequency);
    printf("bits per sample = %u\n", w.bits_per_sample);
    if (w.format != 1) panic("expected format 1, got %u", w.format);
    if (w.channels != 2) panic("expected 2 channels, got %u", w.channels);

    //
    // INFO block
    //
    if (!readinfo(r)) return false;

    //
    // PCM data
    //
    if (!readtag(r, "data")) return false;
    uint32_t datalen = 0;
    endian.read4le(r, &datalen);

    double t = 0;
    double dt = 1.0 / w.frequency;
    int i = 0;
    int bps = w.bits_per_sample / 8;
    for (uint32_t done = 0; done < datalen; done += bps) {
        int left = sample(f);
        int right = sample(f);
        printf("%f\t%d\t%d\n", t, left, right);
        i++;
        t = dt * i;
        if (t >= 1) break;
    }
    reader.free(r);
    fclose(f);
    return true;
}

bool readinfo(reader.t *r) {
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

int sample(FILE *f) {
    int x = 0;
    int a = fgetc(f);
    int b = fgetc(f);
    x = a + 256 * b;
    return x;
}
