#import strings

pub enum { REQUIRED, IMPLIED }

pub enum { IDREF, ID, CDATA }

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

// "<!ATTLIST author          person IDREF #REQUIRED>"
// "<!ATTLIST person          id ID #REQUIRED>",
// "<!ATTLIST profile income CDATA #IMPLIED>",
// "<!ATTLIST edge from IDREF #REQUIRED to IDREF #REQUIRED>"
pub void read_attlist(char *p, attlist_t *attlist) {
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
