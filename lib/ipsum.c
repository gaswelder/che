#import rnd
#import scanner

typedef {
    char *name;
    size_t length;
    char **entries;
} dict_t;

dict_t dicts[100] = {};
size_t ndicts = 0;

dict_t *find_dict(const char *name) {
    for (size_t i = 0; i < ndicts; i++) {
        if (!strcmp(name, dicts[i].name)) {
            return &dicts[i];
        }
    }
    return NULL;
}

pub void add_dict(char *name, size_t length, char **entries) {
    dict_t *d = &dicts[ndicts++];
    d->name = name;
    d->entries = entries;
    d->length = length;
}

pub void emit(const char *s) {
	scanner.t *b = scanner.from_str(s);
	scanner.spaces(b);
	while (scanner.more(b)) {
		if (!parse(b)) {
			char buf[100] = {};
			scanner.tail(b, buf, sizeof(buf));
			panic("[%s]: failed to parse [%s...", s, buf);
		}
		scanner.spaces(b);
	}
	scanner.free(b);
}

bool parse(scanner.t *b) {
	// dict(name)
	if (scanner.skip_literal(b, "dict(")) {
		char name[100] = {};
		if (!scanner.id(b, name, sizeof(name))) {
			return false;
		}
		if (!scanner.skip_literal(b, ")")) {
			return false;
		}
		dict_t *d = find_dict(name);
		if (!d) {
			panic("unknown dict: %s", name);
		}
    	printf("%s", d->entries[rnd.intn(d->length)]);
		return true;
	}

	// irand[10..100]
	if (scanner.skip_literal(b, "irand[")) {
		int n1 = 0;
		int n2 = 0;
		if (!readnum(b, &n1)) return false;
		if (!scanner.skip_literal(b, "..")) return false;
		if (!readnum(b, &n2)) return false;
		if (!scanner.skip_literal(b, "]")) return false;
		printf("%d", n1 + (int) rnd.intn(n2-n1 + 1));
		return true;
	}

	// word
	if (scanner.skip_literal(b, "word")) {
		genword();
		return true;
	}

	// text[1..10]
	if (scanner.skip_literal(b, "text[")) {
		int n1 = 0;
		int n2 = 0;
		if (!readnum(b, &n1)) return false;
		if (!scanner.skip_literal(b, "..")) return false;
		if (!readnum(b, &n2)) return false;
		if (!scanner.skip_literal(b, "]")) return false;
		int n = n1 + (int) rnd.intn(n2);
		for (int i = 0; i < n; i++) {
			int wc = 1 + rnd.intn(4);
			for (int w = 0; w < wc; w++) {
				if (w > 0) putchar(' ');
				genword();
			}
		}
		return true;
	}

	// f2[1.5,15]
	if (scanner.skip_literal(b, "f2[")) {
		double n1 = 0;
		double n2 = 0;
		if (!readfloat(b, &n1)) return false;
		if (!scanner.skip_literal(b, ",")) return false;
		if (!readfloat(b, &n2)) return false;
		if (!scanner.skip_literal(b, "]")) return false;
		double d = n1 + rnd.u() * n2;
        printf("%.2f", d);
		return true;
	}

	// '...'
	if (scanner.skip_literal(b, "'")) {
		char buf[100] = {};
		size_t len = 0;
		while (scanner.more(b) && scanner.peek(b) != '\'') {
			if (len == sizeof(buf)-1) {
				panic("buf too small");
			}
			buf[len++] = scanner.get(b);
		}
		if (!scanner.skip_literal(b, "'")) return false;
		printf("%s", buf);
		return true;
	}
	return false;
}

bool readnum(scanner.t *b, int *r) {
	char buf[10] = {};
	size_t len = 0;
	while (scanner.more(b) && isdigit(scanner.peek(b))) {
		if (len == sizeof(buf) - 1) {
			panic("buffer to small for a number");
		}
		buf[len++] = scanner.get(b);
	}
	if (len == 0) {
		return false;
	}
	return sscanf(buf, "%d", r) == 1;
}

bool readfloat(scanner.t *b, double *r) {
	char buf[10] = {};
	if (!scanner.num(b, buf, sizeof(buf))) return false;
	return sscanf(buf, "%lf", r) == 1;
}

char *prefixes[] = { "a", "en", "de", "un", "pro", "pre",  };
char *suffixes[] = { "ing", "able", "ed", "ity", "ian", "ty", "gst", };
char *syllables[] = {
    "foo",
    "so", "li", "di", "nor", "we", "gi", "ha", "ve",
    "hun", "gar", "sar", "din", "ho", "nor",
    "in", "er", "ti", "on", "at", "es", "en", "re", "st", "ar",
    "al", "te", "ed", "nd", "to", "nt", "is", "or", "it", "as",
    "le", "an", "ma", "se", "ne", "us", "de", "co", "me", "ra",
    "si", "ve", "di", "ri", "ro", "ng", "li", "la", "so", "ta",
    "ec", "hi", "ni", "ca", "ad", "tr", "ac", "om", "et", "no",
    "ha", "el", "pe", "id", "ur", "pr", "ce", "il", "be", "fo",
    "su", "pa", "un", "lo", "po", "em", "wi", "th", "ll", "ch",
    "ea", "ns", "rt", "sa", "mi", "na", "ic", "he", "ge", "rd",
    "ai", "nc", "ul", "mp", "ci", "ou", "io", "am", "sp", "fi",
    "sh", "ld", "ct", "bl", "ck", "gr", "cr", "br", "fr", "sm",
    "ing", "ion", "ent", "ers", "est", "ati", "ter", "con", "res", "and",
    "for", "ted", "nce", "tio", "per", "ant", "all", "ess", "ver", "pro",
    "rea", "son", "sta", "men", "der", "her", "com", "int", "und", "not",
    "end", "cal", "ble", "ard", "are", "ect", "ive", "can", "str", "out",
    "age", "pre", "dis", "but", "ran", "rat", "use", "led", "sis", "tic",
    "ous", "ide", "tor", "rec", "orm", "act", "ist", "one", "ine", "nte",
    "min", "tra", "ght", "lan", "sed", "sic", "art", "par", "tin", "nat",
    "ber", "red", "por", "den", "man", "ial", "ces", "mer", "ons", "nts",
    "ten", "ies", "ven", "lin", "get", "new", "fin", "ord", "sur", "eve",
    "let", "ain", "ind", "sup", "las", "ron", "ite", "sen", "fer", "set",
    "day", "ove", "ime", "way",
};

void genword() {
    if (rnd.u() < 0.5) {
        printf("%s", prefixes[rnd.intn(nelem(prefixes))]);
    }
    syllable();
    if (rnd.u() < 0.5) syllable();
    if (rnd.u() < 0.3) syllable();
    if (rnd.u() < 0.5) {
        printf("%s", suffixes[rnd.intn(nelem(suffixes))]);
    }
}

void syllable() {
    printf("%s", syllables[rnd.intn(nelem(syllables))]);
}
