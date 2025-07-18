// Sloppy XML DTD parser.

#import strings

pub enum {
	REQUIRED = 1,
	IMPLIED = 2,
}

// Attribute types
pub enum {
	ID = 1, // ID attribute (such as <order id="order123">)
	IDREF = 2, // Same as ID, but refers to another element's ID
	CDATA = 3, // Character data ("foo bar")
}

pub typedef {
	char name[100];
	child_list_t *children;
} element_t;

pub typedef {
	bool islist; // if true, data is a child_list_t; if false, data is a child_t.
	void *data;
} child_entry_t;

pub typedef {
	char name[100];
	char quantifier; // '', '*', '?', '+'
} child_t;

pub typedef {
	char quantifier;
	char jointype; // '|', ','
	int size;
	child_entry_t items[100];
} child_list_t;

pub typedef {
	int type, flag;
	char name[100];
	char defval[100];
} att_t;

pub typedef {
	char host[100];
	int size;
	att_t items[10];
} attlist_t;

// Represents the entire parsed definition.
pub typedef {
	element_t elements[100];
	int elemnum;
	attlist_t attlists[100];
	int attrnum;
} schema_t;

pub element_t *get_element(schema_t *s, const char *name) {
	for (int i = 0; i < s->elemnum; i++) {
		element_t *e = &s->elements[i];
		if (strcmp(e->name, name) == 0) {
			return e;
		}
	}
	return NULL;
}

pub attlist_t *get_attributes(schema_t *s, const char *name) {
	for (int i = 0; i < s->attrnum; i++) {
		attlist_t *a = &s->attlists[i];
		if (strcmp(a->host, name) == 0) {
			return a;
		}
	}
	return NULL;
}

pub schema_t parse(const char *real_dtd) {
	schema_t schema = {};
	char buf[200] = {};
	const char *p = real_dtd;
	while (true) {
		p = loadline(p, buf);
		if (buf[0] == '\0') {
			break;
		}
		if (strings.starts_with(buf, "<!ATTLIST ")) {
			attlist_t *attlist = &schema.attlists[schema.attrnum++];
			read_attlist(buf, attlist);
		} else {
			element_t *element = &schema.elements[schema.elemnum++];
			char *p = buf;
			read_element(&p, element);
		}
	}
	return schema;
}

// "<!ATTLIST author          person IDREF #REQUIRED>"
// "<!ATTLIST person          id ID #REQUIRED>",
// "<!ATTLIST profile income CDATA #IMPLIED>",
// "<!ATTLIST edge from IDREF #REQUIRED to IDREF #REQUIRED>"
void read_attlist(char *p, attlist_t *attlist) {
	p = skiplit(p, "<!ATTLIST ");
	while (isspace(*p)) p++;

	strcpy(attlist->host, readname(&p));
	while (isspace(*p)) p++;

	while (*p != '\0' && *p != '>') {
		att_t *att = &attlist->items[attlist->size++];

		// name
		strcpy(att->name, readname(&p));
		while (isspace(*p)) p++;

		// type
		if (strings.starts_with(p, "IDREF")) {
			p = skiplit(p, "IDREF");
			att->type = IDREF;
		} else if (strings.starts_with(p, "ID")) {
			p = skiplit(p, "ID");
			att->type = ID;
		} else if (strings.starts_with(p, "CDATA")) {
			p = skiplit(p, "CDATA");
			att->type = CDATA;
		} else {
			panic("unknown attribute type: %s", p);
		}
		while (isspace(*p)) p++;

		// value
		if (strings.starts_with(p, "#REQUIRED")) {
			p = skiplit(p, "#REQUIRED");
			att->flag = REQUIRED;
		} else if (strings.starts_with(p, "#IMPLIED")) {
			p = skiplit(p, "#IMPLIED");
			att->flag = IMPLIED;
		} else {
			panic("unknown attribute value: %s", p);
		}
		while (isspace(*p)) p++;
	}

	p = skiplit(p, ">");
	if (*p != '\0') {
		panic("unexpected trailing data: %s", p);
	}
}

void read_element(char **s, element_t *element) {
	char *p = *s;

	// <!ELEMENT + spaces
	p = skiplit(p, "<!ELEMENT ");
	while (isspace(*p)) p++;

	// name + spaces
	strcpy(element->name, readname(&p));
	while (isspace(*p)) p++;

	// child list
	element->children = read_child_list(&p);

	p = skiplit(p, ">");

	if (*p != '\0') {
		panic("unexpected trailing data: %s", p);
	}
	*s = p;
}

pub void print_element(element_t *element) {
	printf("element %s:\n", element->name);
	child_list_t *l = element->children;
	for (int i = 0; i < l->size; i++) {
		printf("- child %d: ", i);
		if (l->items[i].islist) {
			printf("list\n");
		} else {
			child_t *e = l->items[i].data;
			printf("elem %s\n", e->name);
		}
	}
}

child_list_t *read_child_list(char **s) {
	child_list_t *list = calloc!(1, sizeof(child_list_t));

	char *p = *s;

	if (strings.starts_with(p, "EMPTY")) {
		p = skiplit(p, "EMPTY");
		while (isspace(*p)) p++;
		*s = p;
		return list;
	}

	p = skiplit(p, "(");
	while (*p != '\0') {
		child_entry_t *it = &list->items[list->size++];
		item(&p, it);
		while (isspace(*p)) p++;

		// If the list kind is not known yet, see if a join symbol follows.
		char peek = *p;
		if (list->jointype == '\0') {
			if (peek == ',' || peek == '|') {
				list->jointype = *p++;
				while (isspace(*p)) p++;
				continue;
			}
			break;
		}

		// If we already know the join symbol, accept only it.
		if (peek == list->jointype) {
			p++;
			while (isspace(*p)) p++;
			continue;
		}
		break;
	}

	p = skiplit(p, ")");
	list->quantifier = mb_quant(&p);
	*s = p;
	return list;
}

void item(char **s, child_entry_t *item) {
	char *p = *s;
	if (*p == '(') {
		item->islist = true;
		item->data = read_child_list(s);
		return;
	}

	child_t *elem = calloc!(1, sizeof(child_t));
	if (strings.starts_with(p, "#PCDATA")) {
		p = skiplit(p, "#PCDATA");
		strcpy(elem->name, "#PCDATA");
	} else {
		strcpy(elem->name, readname(&p));
		elem->quantifier = mb_quant(&p);
	}
	*s = p;
	item->data = elem;
}

char mb_quant(char **s) {
	char *p = *s;
	char q = '\0';
	switch (*p) {
		case '?', '*', '+': { q = *p++; }
	}
	*s = p;
	return q;
}

char *skiplit(char *line, *lit) {
	if (!strings.starts_with(line, lit)) {
		panic("expected %s, got %s", lit, line);
	}
	return line + strlen(lit);
}

char *readname(char **s) {
	char *name = calloc!(100, 1);
	char *p = *s;
	char *n = name;
	while (isalpha(*p) || *p == '_') *n++ = *p++;
	*s = p;
	return name;
}

const char *loadline(const char *p, char *buf) {
	while (isspace(*p)) p++;
	char *b = buf;
	while (*p != '\0' && *p != '\n') {
		*b++ = *p++;
	}
	*b = '\0';
	return p;
}
