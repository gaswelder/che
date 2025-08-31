
pub typedef { double left, right; } samplef_t;

pub typedef {
	uint32_t freq; // 44100
	samplef_t *samples;
	size_t nsamples;
	size_t cap;
} clip_t;

// Creates a new empty clip.
pub clip_t *newclip(size_t freq) {
	clip_t *c = calloc!(1, sizeof(clip_t));
	c->freq = freq;
	c->cap = 65536;
	c->samples = calloc!(c->cap, sizeof(samplef_t));
	return c;
}

pub void freeclip(clip_t *c) {
	free(c->samples);
	free(c);
}

// Adds a sample to clip c.
pub void push_sample(clip_t *c, samplef_t s) {
	if (c->nsamples + 1 > c->cap) {
		c->cap *= 2;
		c->samples = realloc(c->samples, c->cap * sizeof(samplef_t));
		if (!c->samples) {
			panic("realloc failed");
		}
	}
	c->samples[c->nsamples++] = s;
}

// Normalizes clip c to the given max amplitude.
pub void normalize(clip_t *c, double level) {
	double max = 0;
	for (size_t i = 0; i < c->nsamples; i++) {
		samplef_t *s = &c->samples[i];
		if (fabs(s->left) > max) max = fabs(s->left);
		if (fabs(s->right) > max) max = fabs(s->right);
	}
	double k = level / max;
	for (size_t i = 0; i < c->nsamples; i++) {
		samplef_t *s = &c->samples[i];
		s->left *= k;
		s->right *= k;
	}
}
