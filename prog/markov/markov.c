#import strings
#import stats
#import clip/map
#import utf

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

model_t *new() {
	model_t *m = calloc(1, sizeof(model_t));
	if (!m) panic("calloc failed");
	m->counts = map.new(sizeof(void *));
	return m;
}

void learnword(model_t *m, const char *word) {
	// Split the word into chars.
	size_t n = utf.runecount(word);
	utf.Rune *chars = calloc(n, sizeof(utf.Rune));
	if (!chars) panic("calloc failed");
	const char *s = word;
	for (size_t i = 0; i < n; i++) {
		utf.Rune c = 0;
		s += utf.get_rune(&c, s);
		chars[i] = c;
	}

	// Process the pairs.
	add_pair(m, 0, chars[0]);
	add_pair(m, chars[n-1], 0);
    for (size_t i = 1; i < n; i++) {
        add_pair(m, chars[i-1], chars[i]);
    }
}

void putrune(utf.Rune c) {
	char buf[4];
	int size = utf.runetochar(buf, &c);
	buf[size] = 0;
	printf("%s", buf);
}

void add_pair(model_t *m, utf.Rune a, b) {
	// Get the map of completions for a.
	// If no map yet, create one.
	map.map_t *m2 = NULL;
	if (!map.get(m->counts, (uint8_t *)&a, sizeof(a), &m2)) {
		m2 = map.new(sizeof(size_t));
		map.set(m->counts, (uint8_t *)&a, sizeof(a), &m2);
	}

	// Increment b's count in the completions map.
	size_t val = 0;
	map.get(m2, (uint8_t *)&b, sizeof(b), &val);
	val++;
	map.set(m2, (uint8_t *)&b, sizeof(b), &val);
}

void genword(model_t *m) {
    utf.Rune c = 0;
    while (true) {
        c = nextchar(m, c);
        if (!c) break;
		putrune(c);
    }
    putchar('\n');
}

utf.Rune nextchar(model_t *m, utf.Rune c) {
	//
	// Pull out all possible completions and their frequencies.
	//
	map.map_t *m2 = NULL;
	if (!map.get(m->counts, (uint8_t *)&c, sizeof(c), &m2)) {
		panic("failed to get submap");
	}
	size_t size = map.size(m2);
	utf.Rune *keys = calloc(size, sizeof(utf.Rune));
	size_t *freqs = calloc(size, sizeof(size_t));
	map.iter_t *it = map.iter(m2);
	int pos = 0;
	while (map.next(it)) {
		map.itkey(it, (uint8_t *)&keys[pos], sizeof(utf.Rune));
		map.itval(it, &freqs[pos]);
		pos++;
	}
	map.end(it);

	//
	// Select a completion.
	//
	pos = stats.sample_from_distribution(freqs, size);
	utf.Rune next = keys[pos];

	// Cleanup
	free(keys);
	free(freqs);

	return next;
}
