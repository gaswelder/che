
pub typedef { double left, right; } samplef_t;

pub typedef {
	samplef_t *samples;
	size_t nsamples;
	size_t cap;
} clip_t;

// Creates a new empty clip.
pub clip_t *newclip() {
	clip_t *c = calloc!(1, sizeof(clip_t));
	c->cap = 65536;
	c->samples = calloc!(c->cap, sizeof(samplef_t));
	return c;
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
		if (s->left > max) max = s->left;
		if (s->right > max) max = s->right;
	}
	double k = level / max;
	for (size_t i = 0; i < c->nsamples; i++) {
		samplef_t *s = &c->samples[i];
		s->left *= k;
		s->right *= k;
	}
}
