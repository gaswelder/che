#import clip/vec
#import formats/wav
#import midilib.c
#import sound
#import strings
#import rnd

typedef {
	size_t i, n;
	midilib.event_t *ee;
} midireader_t;

midilib.event_t *next(midireader_t *r) {
	if (r->i >= r->n) return NULL;
	return &r->ee[r->i++];
}

void initreader(midireader_t *r, const char *path) {
	r->i = 0;
	midilib.read_file(path, &r->ee, &r->n);
}

void freereader(midireader_t *r) {
	free(r->ee);
}

pub int run(int argc, char *argv[]) {
	if (argc != 4) {
		fprintf(stderr, "usage: %s midi-file track-name font-path\n", argv[0]);
		return 1;
	}
	const char *path = argv[1];
	const char *trackname = argv[2];
	char *fontpath = argv[3];

	load_clips(fontpath);

	composer_t c = {};
	cmpinit(&c, path, trackname);

	wav.writer_t *out = wav.open_writer(stdout);
	sound.samplef_t s = {};
	while (cmpnext(&c, &s)) {
		wav.write_sample(out, s.left, s.right);
	}
	wav.close_writer(out);

	cmpdone(&c);
	return 0;
}

typedef {
	uint8_t key;
	size_t t1, t2; // begin and end time in pcm ticks (1/44100 s)
	sound.samplef_t *samples;
	size_t nsamples;
} range_t;

typedef {
	uint8_t key;
	sound.clip_t *clip;
} clip_t;

clip_t clips[200] = {};
size_t nclips = 0;

sound.clip_t *loadclip(char *path) {
	sound.clip_t *c = wav.loadfile(path);
	if (!c) {
		panic("failed to load %s: %s", path, strerror(errno));
	}
	if (c->freq != 44100) {
		panic("%s: expected frequency %u, got %u", path, 44100, c->freq);
	}
	sound.normalize(c, 1.0/10);
	return c;
}

clip_t *get_clip(uint8_t key) {
	for (size_t i = 0; i < nclips; i++) {
		if (clips[i].key == key) {
			return &clips[i];
		}
	}

	// If we don't have one, choose a random sample to transpose.
	clip_t *c = &clips[rnd.intn(nclips)];
	fprintf(stderr, "getting %u from %u\n", key, c->key);

	// Physical frequency of a midi note is k * 2^(note/12).
	// The ratio between the two is then:
	float r = pow(2, 1.0/12 * (c->key - key));
	return addclip(sound.transpose(c->clip, r), key);
}

void load_clips(char *path) {
	FILE *f = fopen(path, "rb");
	if (!f) {
		panic("failed to open clips list file\n");
	}
	char line[4096] = {};
	while (fgets(line, 4096, f)) {
		char *p = line;
		int key = 0;
		while (isdigit(*p)) {
			key *= 10;
			key += strings.num_from_ascii(*p++);
		}
		while (isspace(*p)) p++;
		strings.trim(p);

		// Load the clip
		addclip(loadclip(p), key);
		fprintf(stderr, "loaded %s\n", p);
	}
	fclose(f);
}

clip_t *addclip(sound.clip_t *sc, uint8_t key) {
	if (nclips == nelem(clips)) {
		panic("reached clips limit");
	}
	clip_t *c = &clips[nclips++];
	c->key = key;
	c->clip = sc;
	return c;
}

typedef {
	size_t nranges;
	size_t tmax;
	size_t i;
	vec.t *ranges;
} composer_t;

void cmpinit(composer_t *c, const char *path, *trackname) {
	midireader_t r = {};
	initreader(&r, path);

	//
	// Find the track number.
	//
	uint8_t track = 127;
	for (size_t i = 0; i < r.n; i++) {
		midilib.event_t *e = &r.ee[i];
		if (e->type == midilib.TRACK_NAME) {
			if (strcmp(e->str, trackname) == 0) {
				track = e->track;
				break;
			}
		}
	}
	if (track == 127) {
		panic("couldn't find track named '%s'", trackname);
	}

	c->ranges = vec.new(sizeof(range_t));

	while (true) {
		midilib.event_t *e = next(&r);
		if (!e) break;
		if (e->track != track) continue;

		switch (e->type) {
			case midilib.NOTE_ON: {
				range_t *r = vec.alloc(c->ranges);
				r->t1 = e->t_us * 44100 / 1000000;
				r->key = e->key;
				clip_t *c = get_clip(e->key);
				if (!c) {
					fprintf(stderr, "no clip for key %u\n", e->key);
				} else {
					r->samples = c->clip->samples;
					r->nsamples = c->clip->nsamples;
				}
			}
			case midilib.NOTE_OFF: {
				// Find the first unclosed matching range.
				range_t *r = NULL;
				size_t nranges = vec.len(c->ranges);
				for (size_t i = 0; i < nranges; i++) {
					range_t *x = vec.index(c->ranges, i);
					if (x->key == e->key && x->t2 == 0) {
						r = x;
						break;
					}
				}
				if (!r) {
					panic("failed to find the open range to close");
				}
				r->t2 = e->t_us * 44100 / 1000000;
				size_t tlen = r->t2 - r->t1;
				if (r->nsamples > tlen) {
					r->t2 = r->t1 + r->nsamples;
					// fprintf(stderr, "trimmed sample %u: %zu vs %zu\n", r->key, tlen, r->nsamples);
				}
			}
		}
	}

	freereader(&r);
	c->nranges = vec.len(c->ranges);
	size_t tmax = 0;
	for (size_t i = 0; i < c->nranges; i++) {
		range_t *r = vec.index(c->ranges, i);
		// fprintf(stderr, "%zu: %zu..%zu: %u\n", i, r->t1, r->t2, r->key);
		if (r->t2 > tmax) {
			tmax = r->t2;
		}
	}
	c->tmax = tmax;
}

bool cmpnext(composer_t *c, sound.samplef_t *s) {
	if (c->i == c->tmax) return false;
	// This is a brute-force scan from 0 to the max time,
	// where for each time instant we query all ranges.
	double left = 0;
	double right = 0;
	for (size_t j = 0; j < c->nranges; j++) {
		range_t *r = vec.index(c->ranges, j);
		if (c->i < r->t1 || c->i >= r->t2) continue;
		size_t pos = c->i - r->t1;
		if (pos >= r->nsamples) continue;
		left += r->samples[pos].left;
		right += r->samples[pos].right;
	}
	s->left = left;
	s->right = right;
	c->i++;
	return true;
}


void cmpdone(composer_t *c) {
	free(c->ranges);
}
