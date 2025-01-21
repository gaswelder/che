#import strings
#import stats

// Reads lines of ASCII text, counts character pair frequencies,
// generates lines from the frequencies.

int main() {
    char word[100];
    size_t *counts = calloc(256*256, sizeof(size_t));
    if (!counts) panic("calloc failed");

    while (fgets(word, sizeof(word), stdin)) {
        strings.rtrim(word, "\n");
        learnword(counts, word);
    }

    for (int i = 0; i < 10; i++) {
        genword(counts);
    }
    return 0;
}

void learnword(size_t *counts, char *word) {
    size_t n = strlen(word);
    addpair(counts, 0, (uint8_t) word[0]);
    addpair(counts, (uint8_t) word[n-1], 0);
    for (size_t i = 1; i < n; i++) {
        addpair(counts, (uint8_t) word[i-1], (uint8_t) word[i]);
    }
}

void addpair(size_t *counts, uint8_t a, b) {
    counts[a * 256 + b]++;
}

void genword(size_t *counts) {
    uint8_t c = 0;
    while (true) {
        c = nextchar(counts, c);
        if (!c) break;
        printf("%c", c);
    }
    putchar('\n');
}

uint8_t nextchar(size_t *counts, uint8_t c) {
    // Get a list of frequencies for completions of c
	size_t freqs[256] = {};
	size_t *p = &counts[c*256];
    for (int i = 0; i < 256; i++) {
        freqs[i] = *p++;
    }
	// Sample and return a completion.
    return stats.sample_from_distribution(freqs, 256);
}
