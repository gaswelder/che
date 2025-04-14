#import strings
#import strbuilder

pub typedef {
	bool list;
	bool num;
	void *data;
	item_t *next;
} item_t;

// Creates a new symbol with the given text content.
pub item_t *sym(char *s) {
	item_t *i = calloc(1, sizeof(item_t));
	i->data = s;
	return i;
}

// Constructs a new list [...data, next].
pub item_t *cons(item_t *data, item_t *next) {
	item_t *r = calloc(1, sizeof(item_t));
	r->list = true;
	r->data = data;
	r->next = next;
	return r;
}

pub item_t *reverse(item_t *in) {
	item_t *r = NULL;
	item_t *l = in;
	while (l) {
		r = cons(car(l), r);
		l = cdr(l);
	}
	return r;
}

pub item_t *car(item_t *x) {
	if (!x) {
		panic("car: NULL");
	}
	if (!x->list) {
		dbgprint(x);
		panic("car: item not a list");
	}
	return x->data;
}

// Prints the given item to stdout for debugging.
pub void dbgprint(item_t *x) {
	char buf[4096];
	print(x, buf, 4096);
	printf("%p (data %p) %s\n", (void *)x, x->data, buf);
}

pub item_t *cdr(item_t *x) {
	if (!x) {
		panic("cdr: NULL");
	}
	if (!x->list) {
		dbgprint(x);
		panic("cdr: item not a list: %s", desc(x));
	}
	return x->next;
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

void _print(strbuilder.str *s, item_t *x) {
	if (!x) {
		strbuilder.adds(s, "NULL");
		return;
	}
	if (!x->list) {
		strbuilder.adds(s, x->data);
		return;
	}
	strbuilder.str_addc(s, '(');
	item_t *l = x;
	int i = 0;
	while (l) {
		if (i > 0) {
			strbuilder.str_addc(s, ' ');
		}
		_print(s, car(l));
		l = cdr(l);
		i++;
	}
	strbuilder.str_addc(s, ')');
}

pub void print(item_t *x, char *buf, size_t len) {
	strbuilder.str *s = strbuilder.str_new();
	_print(s, x);
	char *r = strbuilder.str_unpack(s);

	strncpy(buf, r, len);
	free(r);
}

char *desc(item_t *x) {
	if (!x) {
		return "NULL";
	}
	if (x->list) {
		return strings.newstr("list (%s, %s)", desc(x->data), desc(x->next));
	}
	return strings.newstr("sym(%s)", x->data);
}
