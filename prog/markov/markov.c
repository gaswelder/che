#import strings
#import stats

// Reads lines of ASCII text, counts character pair frequencies,
// generates lines from the frequencies.

typedef {
	size_t *counts;
} model_t;

int main() {
    char word[100];
	model_t m = {
		.counts = calloc(256*256, sizeof(size_t))
	};
    if (!m.counts) panic("calloc failed");

    while (fgets(word, sizeof(word), stdin)) {
        strings.rtrim(word, "\n");
        learnword(&m, word);
    }

    for (int i = 0; i < 10; i++) {
        genword(&m);
    }
    return 0;
}

void learnword(model_t *m, char *word) {
    size_t n = strlen(word);
    set_count(m, 0, (uint8_t) word[0]);
    set_count(m, (uint8_t) word[n-1], 0);
    for (size_t i = 1; i < n; i++) {
        set_count(m, (uint8_t) word[i-1], (uint8_t) word[i]);
    }
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
    // Get a list of frequencies for completions of c
	size_t freqs[256] = {};
    for (int i = 0; i < 256; i++) {
        freqs[i] = get_count(m, c, i);
    }
	// Sample and return a completion.
    return stats.sample_from_distribution(freqs, 256);
}

void set_count(model_t *m, uint8_t a, b) {
    m->counts[a * 256 + b]++;
}

size_t get_count(model_t *m, uint8_t a, b) {
	return m->counts[a * 256 + b];
}
