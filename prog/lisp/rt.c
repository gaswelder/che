#import strings
#import strbuilder

pub item_t *reverse(item_t *in) {
	item_t *r = NULL;
	item_t *l = in;
	while (l) {
		r = cons(car(l), r);
		l = cdr(l);
	}
	return r;
}

char *symbols[1000] = {0};

// Interns the given text and returns the interned pointer.
pub char *intern(char *text) {
	int i = 0;
	while (i < 1000 && symbols[i]) {
		char *s = symbols[i];
		if (strcmp(s, text) == 0) {
			return s;
		}
		i++;
	}
	if (i == 1000) {
		panic("too many symbols");
	}
	symbols[i] = strings.newstr("%s", text);
	return symbols[i];
}
