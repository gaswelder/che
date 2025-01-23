#import strings
#import stats
#import clip/map

// Reads lines of ASCII text, counts character pair frequencies,
// generates lines from the frequencies.

typedef {
	map.map_t *counts;
} model_t;

int main() {
	model_t *m = new();

    char word[100];
    while (fgets(word, sizeof(word), stdin)) {
        strings.rtrim(word, "\n");
        learnword(m, word);
    }

    for (int i = 0; i < 10; i++) {
        genword(m);
    }
    return 0;
}

const size_t HACK = 8; // sizeof(void *)

model_t *new() {
	model_t *m = calloc(1, sizeof(model_t));
	if (!m) panic("calloc failed");
	m->counts = map.new(HACK);
	return m;
}

void learnword(model_t *m, char *word) {
    size_t n = strlen(word);
    add_pair(m, 0, (uint8_t) word[0]);
    add_pair(m, (uint8_t) word[n-1], 0);
    for (size_t i = 1; i < n; i++) {
        add_pair(m, (uint8_t) word[i-1], (uint8_t) word[i]);
    }
}

void add_pair(model_t *m, uint8_t a, b) {
	// Get the map of completions for a.
	// If no map yet, create one.
	map.map_t *m2 = NULL;
	if (!map.get(m->counts, &a, 1, &m2)) {
		m2 = map.new(sizeof(size_t));
		map.set(m->counts, &a, 1, &m2);
	}

	// Increment b's count in the completions map.
	size_t val = 0;
	map.get(m2, &b, 1, &val);
	val++;
	map.set(m2, &b, 1, &val);
}

void genword(model_t *m) {
    uint8_t c = 0;
    while (true) {
        c = nextchar(m, c);
        if (!c) break;
        printf("%c", c);
    }
    putchar('\n');
}

uint8_t nextchar(model_t *m, uint8_t c) {
	//
	// Pull out all possible completions and their frequencies.
	//
	map.map_t *m2 = NULL;
	if (!map.get(m->counts, &c, 1, &m2)) {
		panic("failed to get submap");
	}
	size_t size = map.size(m2);
	uint8_t *keys = calloc(size, 1);
	size_t *freqs = calloc(size, sizeof(size_t));
	map.iter_t *it = map.iter(m2);
	int pos = 0;
	while (map.next(it)) {
		map.itkey(it, &keys[pos], 1);
		map.itval(it, &freqs[pos]);
		pos++;
	}
	map.end(it);

	//
	// Select a completion.
	//
	pos = stats.sample_from_distribution(freqs, size);
	uint8_t next = keys[pos];

	// Cleanup
	free(keys);
	free(freqs);

	return next;
}
