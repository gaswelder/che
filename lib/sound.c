
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

// Returns a copy of clip c stretched by factor of r.
// r > 1 results in lower sound, r < 1 in higher.
pub clip_t *transpose(clip_t *c, float r) {
	clip_t *c2 = newclip(c->freq);

	// How many samples the new clip will have.
	size_t n2 = (size_t) ((float) c->nsamples * r);

	for (size_t t = 0; t < n2; t++) {
		// The "time" in the original sample flows r times slower.
		float t0 = (float) t / r;

		// Split the fractional time into an integer and a remainder.
		// t0 = 13.156 means 0.156 between samples 13 and 14.
		size_t t0_int = (size_t) t0;
		float b = t0 - (float) t0_int;
		float a = 1 - b;

		if (t0_int >= c->nsamples) {
			panic("oopsie, %zu >= %zu (at t=%zu)\n", t0_int, c->nsamples, t);
		}

		// Mix two original samples in the proportion according
		// to the remainder in t.
		samplef_t *s = &c->samples[t0_int];
		double left = s->left * a;
		double right = s->right * a;
		if (b > 0) {
			s = &c->samples[t0_int+1];
			left += s->left * b;
			right += s->right * b;
		}
		samplef_t t = {left, right};
		push_sample(c2, t);
	}
	return c2;
}
