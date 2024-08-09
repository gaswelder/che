pub typedef {
	FILE *f;
} t;

pub void byte(t *w, uint8_t b) {
	// fwrite(&b, 1, 1, w->f);
	fputc(b, w->f);
}

pub void le16(t *w, uint16_t v) {
	// byte(w, v % 256); v /= 256;
	// byte(w, v % 256); v /= 256;
	fputc((v >> 0) & 0xff, w->f);
    fputc((v >> 8) & 0xff, w->f);
}

pub void le32(t *w, uint32_t v) {
	// byte(w, v % 256); v /= 256;
	// byte(w, v % 256); v /= 256;
	// byte(w, v % 256); v /= 256;
	// byte(w, v % 256); v /= 256;
	fputc((v >>  0) & 0xff, w->f);
    fputc((v >>  8) & 0xff, w->f);
    fputc((v >> 16) & 0xff, w->f);
    fputc((v >> 24) & 0xff, w->f);
}

pub void be32(t *w, uint32_t v) {
    fputc((v >> 24) & 0xff, w->f);
    fputc((v >> 16) & 0xff, w->f);
    fputc((v >>  8) & 0xff, w->f);
    fputc((v >>  0) & 0xff, w->f);
}

