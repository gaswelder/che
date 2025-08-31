#import fft
#import complex
#import formats/wav
#import sound

pub int run(int argc, char** argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s input.wav ir.wav output.wav\n", argv[0]);
        return 1;
    }

	sound.clip_t *in = wav.loadfile(argv[1]);
	if (!in) panic("failed to load %s", argv[1]);
	printf("loaded %s\n", argv[1]);

	sound.clip_t *ir = wav.loadfile(argv[2]);
	if (!ir) panic("failed to load %s", argv[2]);
	printf("loaded %s\n", argv[2]);

    sound.clip_t *out = fft_convolve(in, ir);
    sound.normalize(out, 1.0);
	writeclip(argv[3], out);

    sound.freeclip(in);
	sound.freeclip(ir);
	sound.freeclip(out);
    return 0;
}

void writeclip(char *path, sound.clip_t *c) {
	FILE* f = fopen(path, "wb");
    if (!f) { panic("open"); }
	wav.writer_t *out = wav.open_writer(f);
	for (size_t i = 0; i < c->nsamples; i++) {
        double s = OS.fmax(-1.0, OS.fmin(1.0, c->samples[i].left));
        wav.write_sample(out, s, s);
    }
	wav.close_writer(out);
    fclose(f);
}

// FFT-based convolution (naive zero-padded)
sound.clip_t *fft_convolve(sound.clip_t *x, *h) {
    size_t Ny = x->nsamples + h->nsamples - 1;
    size_t Nfft = next_pow2(Ny);

	printf("starting fft\n");
    fft.kiss_fft_state_t *cfg = fft.kiss_fft_alloc(Nfft, 0, NULL, NULL);
	fft.kiss_fft_state_t *icfg = fft.kiss_fft_alloc(Nfft, 1, NULL, NULL);

	printf("fft x...\n");
    complex.t *X = calloc!(Nfft, sizeof(complex.t));
	for (size_t i = 0; i < x->nsamples; i++) {
		X[i].re = x->samples[i].left;
	}
	fft.kiss_fft(cfg, X, X);

	printf("fft h...\n");
    complex.t *H = calloc!(Nfft, sizeof(complex.t));
	for (size_t i = 0; i < h->nsamples; i++) {
		H[i].re = h->samples[i].left;
	}
	fft.kiss_fft(cfg, H, H);

	printf("multiplying...\n");
    complex.t *Y = calloc!(Nfft, sizeof(complex.t));
    for (size_t i = 0; i < Nfft; i++) {
		Y[i] = complex.mul(X[i], H[i]);
    }

	printf("inverse fft...\n");
    fft.kiss_fft(icfg, Y, Y);

    // Extract real part, scale by Nfft
	sound.clip_t *r = sound.newclip(44100);
    for (size_t i = 0; i < Ny; i++) {
		double v = Y[i].re / Nfft;
		sound.samplef_t s = { v, v };
		sound.push_sample(r, s);
    }

    free(cfg);
    free(icfg);
    free(X);
    free(H);
    free(Y);
    return r;
}

// Next power of two
size_t next_pow2(size_t n) {
    size_t p = 1;
    while (p < n) {
        p = p << 1;
    }
    return p;
}
