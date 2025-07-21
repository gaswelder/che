#import formats/dtd
#import ipsum.c
#import opt
#import rnd

const char *real_dtd = "
	<!ELEMENT site (regions, categories, catgraph, people, open_auctions, closed_auctions)>
	<!ELEMENT categories (category+)>
	<!ELEMENT regions (africa, asia, australia, europe, namerica, samerica)>
	<!ELEMENT europe  (item*)>
	<!ELEMENT australia	   (item*)>
	<!ELEMENT africa  (item*)>
	<!ELEMENT namerica		(item*)>
	<!ELEMENT samerica		(item*)>
	<!ELEMENT asia	(item*)>
	<!ELEMENT catgraph		(edge*)>
	<!ELEMENT edge EMPTY>
	<!ATTLIST edge from IDREF #REQUIRED to IDREF #REQUIRED>
	<!ELEMENT category (name, description)>
	<!ATTLIST category		id ID #REQUIRED>
	<!ELEMENT item (location, quantity, name, payment, description, shipping, incategory+, mailbox)>
	<!ATTLIST item			id ID #REQUIRED featured CDATA #IMPLIED>
	<!ELEMENT location		(#PCDATA)>
	<!ELEMENT quantity		(#PCDATA)>
	<!ELEMENT payment (#PCDATA)>
	<!ELEMENT name	(#PCDATA)>
	<!ELEMENT name	(#PCDATA)>
	<!ELEMENT name	(#PCDATA)>
	<!ELEMENT description	 (text | parlist)>
	<!ELEMENT parlist	  (listitem*)>
	<!ELEMENT text	(#PCDATA)>
	<!ELEMENT listitem		(text | parlist)*>
	<!ELEMENT shipping		(#PCDATA)>
	<!ELEMENT reserve (#PCDATA)>
	<!ELEMENT incategory	  EMPTY>
	<!ATTLIST incategory	  category IDREF #REQUIRED>
	<!ELEMENT mailbox (mail*)>
	<!ELEMENT mail (from, to, date, text)>
	<!ELEMENT from	(#PCDATA)>
	<!ELEMENT to	  (#PCDATA)>
	<!ELEMENT date	(#PCDATA)>
	<!ELEMENT people  (person*)>
	<!ELEMENT person  (name, emailaddress, phone?, address?, homepage?, creditcard?, profile?, watches?)>
	<!ATTLIST person		  id ID #REQUIRED>
	<!ELEMENT emailaddress	(#PCDATA)>
	<!ELEMENT phone   (#PCDATA)>
	<!ELEMENT homepage		(#PCDATA)>
	<!ELEMENT creditcard	  (#PCDATA)>
	<!ELEMENT address (street, city, country, province?, zipcode)>
	<!ELEMENT street  (#PCDATA)>
	<!ELEMENT city	(#PCDATA)>
	<!ELEMENT province		(#PCDATA)>
	<!ELEMENT zipcode (#PCDATA)>
	<!ELEMENT country (#PCDATA)>
	<!ELEMENT profile (interest*, education?, gender?, business, age?)>
	<!ATTLIST profile income CDATA #IMPLIED>
	<!ELEMENT education	   (#PCDATA)>
	<!ELEMENT income  (#PCDATA)>
	<!ELEMENT gender  (#PCDATA)>
	<!ELEMENT business		(#PCDATA)>
	<!ELEMENT age	 (#PCDATA)>
	<!ELEMENT interest		EMPTY>
	<!ATTLIST interest		category IDREF #REQUIRED>
	<!ELEMENT watches (watch*)>
	<!ELEMENT watch		   EMPTY>
	<!ATTLIST watch		   open_auction IDREF #REQUIRED>
	<!ELEMENT open_auctions   (open_auction*)>
	<!ELEMENT open_auction	(initial, reserve?, bidder*, current, privacy?, itemref, seller, annotation, quantity,type, interval)>
	<!ATTLIST open_auction	id ID #REQUIRED>
	<!ELEMENT privacy (#PCDATA)>
	<!ELEMENT amount  (#PCDATA)>
	<!ELEMENT current (#PCDATA)>
	<!ELEMENT increase		(#PCDATA)>
	<!ELEMENT type	(#PCDATA)>
	<!ELEMENT itemref		 EMPTY>
	<!ATTLIST itemref		 item IDREF #REQUIRED>
	<!ELEMENT bidder (date, time, personref, increase)>
	<!ELEMENT time	(#PCDATA)>
	<!ELEMENT status (#PCDATA)>
	<!ELEMENT initial (#PCDATA)>
	<!ELEMENT personref	   EMPTY>
	<!ATTLIST personref	   person IDREF #REQUIRED>
	<!ELEMENT seller		  EMPTY>
	<!ATTLIST seller		  person IDREF #REQUIRED>
	<!ELEMENT interval		(start, end)>
	<!ELEMENT start   (#PCDATA)>
	<!ELEMENT end	 (#PCDATA)>
	<!ELEMENT closed_auctions (closed_auction*)>
	<!ELEMENT closed_auction  (seller, buyer, itemref, price, date, quantity, type, annotation?)>
	<!ELEMENT price   (#PCDATA)>
	<!ELEMENT buyer		   EMPTY>
	<!ATTLIST buyer		   person IDREF #REQUIRED>
	<!ELEMENT annotation	  (author, description?, happiness)>
	<!ELEMENT happiness	   (#PCDATA)>
	<!ELEMENT author EMPTY>
	<!ATTLIST author		  person IDREF #REQUIRED>";

const char *mapgen(const char *elemname) {
	if (!elemname) return NULL;
	switch str (elemname) {
        case "city": { return "dict(cities)"; }
        case "status", "happiness": { return "irand[1..10]"; }
        case "age": { return "irand[18..45]"; }
        case "amount", "price": { return "price"; }
        case "business", "privacy": { return "yesno"; }
        case "name": { return "sentence1"; }
        case "creditcard": { return "irand[1000..9999] irand[1000..9999] irand[1000..9999] irand[1000..9999]"; }
        case "current": { return "current"; }
        case "education": { return "education"; }
        case "email": { return "email"; }
        case "from", "to", "name": { return "name"; }
        case "gender": { return "gender"; }
        case "homepage": { return "webpage"; }
        case "increase": { return "increase"; }
        case "initial": { return "init_price"; }
        case "location", "country": { return "location"; }
        case "payment": { return "payment"; }
        case "phone": { return "phone"; }
        case "province": { return "province"; }
        case "quantity": { return "quantity"; }
        case "reserve": { return "reserve"; }
        case "shipping": { return "dict(shipping)"; }
        case "street": { return "street"; }
        case "text": { return "any"; }
        case "time": { return "time"; }
        case "type": { return "type"; }
        case "date", "start", "end": { return "date"; }
        case "zipcode": { return "zip"; }
    }
	return "any";
}

int main(int argc, char **argv) {
    opt.parse(argc, argv);

    dtd.schema_t schema = dtd.parse(real_dtd);
    fprintf(stdout, "<?xml version=\"1.0\" standalone=\"yes\"?>\n");
	emit_element(&schema, "site", 0);
    return 0;
}

// Emits a tree for element with the specified name.
void emit_element(dtd.schema_t *s, const char *name, int level) {
	if (level > 20) return;
	indent(level);

	dtd.element_t *e = dtd.get_element(s, name);
	if (!e) panic("element not found: %s", name);

	printf("<%s", name);
	dtd.attlist_t *a = dtd.get_attributes(s, name);
	if (a) {
		for (int i = 0; i < a->size; i++) {
			emit_attr(&a->items[i]);
		}
	}

	// If this element contains only text, format it in one line.
	if (children_only_cdata(e)) {
		printf(">");
		emit_child_list(s, e, level+1);
		printf("</%s>\n", name);
		return;
	}

	// If no children, format as void tag.
	if (e->children->size == 0) {
		printf(" />\n");
		return;
	}

	printf(">\n");
	emit_child_list(s, e, level + 1);
	indent(level);
	printf("</%s>\n", name);
}

void emit_attr(dtd.att_t *a) {
	printf(" %s=\"%d\"", a->name, rnd.intn(1000000));
}

// Returns true if element e's children are just pcdata.
bool children_only_cdata(dtd.element_t *e) {
	dtd.child_list_t *cl = e->children;
	if (cl->size != 1) return false;
	dtd.child_entry_t *ce = &e->children->items[0];
	if (ce->islist) return false;
	dtd.child_t *c = ce->data;
	return strcmp(c->name, "#PCDATA") == 0;
}

void emit_child_list(dtd.schema_t *s, dtd.element_t *e, int level) {
	dtd.child_list_t *l = e->children;
	int n = 1;
	switch (l->quantifier) {
		case '\0': {}
		case '*': { n = rnd.intn(3); }
		default: {
			panic("unimplemented quantifier: %c", l->quantifier);
		}
	}
	for (int j = 0; j < n; j++) {
		switch (l->jointype) {
			// \0 means the list has only one element.
			case '\0', ',': {
				// (a, b, c) -> emit all in sequence
				for (int i = 0; i < l->size; i++) {
					emit_child_entry(s, e->name, &l->items[i], level);
				}
			}
			case '|': {
				// (a | b | c) -> emit random one
				int i = rnd.intn(l->size);
				emit_child_entry(s, e->name, &l->items[i], level);
			}
			default: {
				panic("unimplemented jointype: %c", l->jointype);
			}
		}
	}
}

void emit_child_entry(dtd.schema_t *s, const char *parent_name, dtd.child_entry_t *ce, int level) {
	if (ce->islist) {
		panic("sublist not implemented");
	}

	dtd.child_t *c = ce->data;
	int n = 1;
	switch (c->quantifier) {
		case '\0': {}
		case '*': { n = rnd.intn(3); }
		case '+': { n = 1 + rnd.intn(2); }
		case '?': { n = rnd.intn(2); }
		default: {
			panic("q = %c", c->quantifier);
		}
	}
	for (int i = 0; i < n; i++) {
		if (strcmp(c->name, "#PCDATA") == 0) {
			ipsum.emit(mapgen(parent_name));
		} else {
			emit_element(s, c->name, level);
		}
	}
}

void indent(int level) {
    for (int i = 0; i < level; i++) {
        putchar(' ');
		putchar(' ');
    }
}
