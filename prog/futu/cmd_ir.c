#import fft
#import complex

pub int run(int argc, char** argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s input.wav ir.wav output.wav\n", argv[0]);
        return 1;
    }

    WAVHeader in_hdr;
    WAVHeader ir_hdr;
    size_t in_len;
    size_t ir_len;
    float* in_data = load_wav(argv[1], &in_hdr, &in_len);
    float* ir_data = load_wav(argv[2], &ir_hdr, &ir_len);

    size_t out_len;
    float* out_data = fft_convolve(in_data, in_len, ir_data, ir_len, &out_len);

    // Normalize
    float maxv = 0;
    for (size_t i = 0; i < out_len; i++)
        if (OS.fabsf(out_data[i]) > maxv) maxv = OS.fabsf(out_data[i]);
    if (maxv > 0) {
        for (size_t i = 0; i < out_len; i++)
            out_data[i] /= maxv;
    }

    save_wav(argv[3], &in_hdr, out_data, out_len);

    free(in_data);
    free(ir_data);
    free(out_data);
    return 0;
}



typedef {
    char riff_id[4];
    uint32_t riff_size;
    char wave_id[4];
    char fmt_id[4];
    uint32_t fmt_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data_id[4];
    uint32_t data_size;
} WAVHeader;

// Read mono 16-bit PCM WAV into float array
float* load_wav(const char* filename, WAVHeader* header, size_t* num_samples) {
    FILE* f = fopen(filename, "rb");
    if (!f) { panic("!"); }

    fread(header, sizeof(WAVHeader), 1, f);
    if (memcmp(header->riff_id, "RIFF", 4) != 0 || memcmp(header->wave_id, "WAVE", 4) != 0) {
        fprintf(stderr, "Not a valid WAV: %s\n", filename);
        exit(1);
    }
    if (header->audio_format != 1 || header->bits_per_sample != 16 || header->num_channels != 1) {
        fprintf(stderr, "Only mono 16-bit PCM supported\n");
        exit(1);
    }

    size_t samples = header->data_size / 2;
    float* data = calloc!(samples, sizeof(float));
    int16_t s;
    for (size_t i = 0; i < samples; i++) {
        fread(&s, sizeof(int16_t), 1, f);
        data[i] = s / 32768.0f;
    }
    fclose(f);
    *num_samples = samples;
	printf("loaded %s\n", filename);
    return data;
}

// Save mono float array to 16-bit PCM WAV
void save_wav(const char* filename, const WAVHeader* ref, float* data, size_t num_samples) {
    FILE* f = fopen(filename, "wb");
    if (!f) { panic("open"); }

    WAVHeader out = *ref;
    out.data_size = num_samples * 2;
    out.riff_size = 36 + out.data_size;

    fwrite(&out, sizeof(WAVHeader), 1, f);
    for (size_t i = 0; i < num_samples; i++) {
        float s = OS.fmaxf(-1.0f, OS.fminf(1.0f, data[i]));
        int16_t si = (int16_t)(s * 32767.0f);
        fwrite(&si, sizeof(int16_t), 1, f);
    }
    fclose(f);
}

// Next power of two
size_t next_pow2(size_t n) {
    size_t p = 1;
    while (p < n) {
        p = p << 1;
    }
    return p;
}

// FFT-based convolution (naive zero-padded)
float* fft_convolve(float* x, size_t Nx, float* h, size_t Nh, size_t* Ny) {
    *Ny = Nx + Nh - 1;
    size_t Nfft = next_pow2(*Ny);

	printf("starting fft\n");
    fft.kiss_fft_state_t *cfg = fft.kiss_fft_alloc(Nfft, 0, NULL, NULL);
    fft.kiss_fft_state_t *icfg = fft.kiss_fft_alloc(Nfft, 1, NULL, NULL);

    complex.t *X = calloc!(Nfft, sizeof(complex.t));
    complex.t *H = calloc!(Nfft, sizeof(complex.t));
    complex.t *Y = calloc!(Nfft, sizeof(complex.t));
	printf("copying\n");
    // Copy input to complex buffer
    for (size_t i = 0; i < Nx; i++) X[i].re = x[i];
    for (size_t i = 0; i < Nh; i++) H[i].re = h[i];

    // FFT both signals
	printf("fft1...\n");
    fft.kiss_fft(cfg, X, X);
	printf("fft2...\n");
    fft.kiss_fft(cfg, H, H);

	printf("%zu\n", Nfft);
    // Multiply in frequency domain
    for (size_t i = 0; i < Nfft; i++) {
        float ar = X[i].re;
        float ai = X[i].im;
        float br = H[i].re;
        float bi = H[i].im;
        Y[i].re = ar * br - ai * bi;
        Y[i].im = ar * bi + ai * br;
    }

    // Inverse FFT
	printf("inverse fft...\n");
    fft.kiss_fft(icfg, Y, Y);

    // Extract real part, scale by Nfft
    float* y = calloc!(*Ny, sizeof(float));
    for (size_t i = 0; i < *Ny; i++) {
        y[i] = Y[i].re / Nfft;
    }

    free(cfg);
    free(icfg);
    free(X);
    free(H);
    free(Y);

    return y;
}

