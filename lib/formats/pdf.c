#import formats/pdflex

pub enum {
	NAME = 1,
	NUM,
	MAP,
	HEX,
	ARR,
	REF,
	TFALSE,
	STR,

	XREFLIST,
	OBJ,
	TRAILER,
	STARTXREF,
}

pub typedef {
	int type;
	char chars[1000]; // if type is NAME
	int num; // if type is NUM

	// Child values for MAP, ARR
	int size;
	int cap;
	val_t *keys;
	val_t *vals;
} val_t;

pub typedef { int id, size; val_t items[2]; } obj_t;
pub typedef { int id, pos; bool in_use; } xref_entry_t;
pub typedef { int n; xref_entry_t *items; } xref_list_t;
pub typedef { val_t val; } trailer_t;
pub typedef { val_t val; } startxref_t;
pub typedef {
	int type;
	union {
		obj_t obj;
		xref_list_t xreflist;
		trailer_t trailer;
		startxref_t startxref;
	} content;
} any_t;

pub typedef {
	pdflex.lexer_t *lex;
	FILE *f, *f2;
	xref_list_t xrefs[10];
	int nxrefs;
} reader_t;

pub reader_t *open(const char *path) {
	reader_t *p = calloc!(1, sizeof(reader_t));	
	p->f = fopen(path, "rb");
	p->f2 = fopen(path, "rb");
	p->lex = pdflex.new(p->f);
	pdflex.init(p->lex);
	return p;
}

pub void close(reader_t *p) {
	fclose(p->f);
	fclose(p->f2);
}

pub bool more(reader_t *p) {
	return pdflex.more(p->lex);
}

pub any_t next(reader_t *p) {
	pdflex.lexer_t *lex = p->lex;
	if (lpeek(lex, "xref")) {
		xref_list_t l = read_xref(lex);
		append_xrefs(p, l);
		any_t val = { .type = XREFLIST };
		val.content.xreflist = l;
		return val;
	}
	if (lpeek(lex, "trailer")) {
		// trailer << /Size 1715 /ID[...] >>
		// means:
		// xrefs table will have 1715 entries, for objids 0..1714.
		// doc id is [..., ...].
		trailer_t t = {};
		must(lex, "trailer");
		t.val = whatever(lex);
		any_t val = { .type = TRAILER };
		val.content.trailer = t;
		return val;
	}
	if (lpeek(lex, "startxref")) {
		// startxref 173 %%EOF
		// means:
		// Current document version ends here.
		// Refs table is at byte 173.
		startxref_t v = {};
		must(lex, "startxref");
		v.val = whatever(lex);
		must(lex, "%%EOF");

		any_t val = { .type = STARTXREF };
		val.content.startxref = v;
		return val;
	}
	any_t val = { .type = OBJ };
	val.content.obj = read_obj1(p);
	return val;
}


void append_xrefs(reader_t *p, xref_list_t list) {
	if (p->nxrefs == 10) {
		panic("ran out of xref list slots");
	}
	p->xrefs[p->nxrefs++] = list;
}

// 1716 0 obj ... endobj
obj_t read_obj0(pdflex.lexer_t *lex) {
	obj_t r = {};
	r.id = mustint(lex);
	int ver = mustint(lex);
	if (ver != 0) {
		panic("unexpected non-zero object ver: %d", ver);
	}
	must(lex, "obj");
	r.items[r.size++] = whatever(lex);
	must(lex, "endobj");
	return r;
}

obj_t read_obj1(reader_t *p) {
	pdflex.lexer_t *lex = p->lex;

	obj_t r = {};
	r.id = mustint(lex);
	int ver = mustint(lex);
	if (ver != 0) {
		panic("unexpected non-zero object ver: %d", ver);
	}
	must(lex, "obj");
	r.items[r.size++] = whatever(lex);
	if (lpeek(lex, "stream")) {
		int len = -1;
		val_t lenval = mapget(r.items[0], "Length");
		switch (lenval.type) {
			case REF: {
				obj_t lenobj = readobjat(p, getrefpos(p, lenval.num));
				if (lenobj.size != 1 || lenobj.items[0].type != NUM) {
					panic("expected a number object");
				}
				len = lenobj.items[0].num;
			}
			case NUM: {
				len = lenval.num;
			}
			default: {
				panic("ref or num expected, got %d", lenval.type);
			}
		}
		must(lex, "stream");
		pdflex.bytes(lex, len);
		must(lex, "endstream");
	}
	must(lex, "endobj");
	return r;
}

obj_t readobjat(reader_t *p, int pos) {
	FILE *f2 = p->f2;
	fseek(f2, pos, SEEK_SET);
	pdflex.lexer_t *l2 = pdflex.new(f2);
	obj_t at = read_obj0(l2);
	pdflex.free(l2);
	return at;
}

val_t mapget(val_t m, const char *key) {
	for (int i = 0; i < m.size; i++) {
		if (m.keys[i].type == NAME && strcmp(m.keys[i].chars, key) == 0) {
			return m.vals[i];
		}
	}
	panic("entry not found");
}

pdflex.tok_t must(pdflex.lexer_t *lex, const char *name) {
	pdflex.tok_t m = pdflex.get(lex);
	if (strcmp(m.name, name) != 0) {
		panic("wanted %s, got %s", name, m.name);
	}
	return m;
}

bool lpeek(pdflex.lexer_t *lex, const char *name) {
	pdflex.tok_t next = pdflex.peek(lex, 0);
	if (strcmp(next.name, name) == 0) {
		return true;
	}
	return false;
}

val_t whatever(pdflex.lexer_t *lex) {
	pdflex.tok_t tok = pdflex.get(lex);
	switch str (tok.name) {
		case "<<": {
			val_t r = { .type = MAP };
			while (true) {
				if (lpeek(lex, ">>")) break;
				val_t k = whatever(lex);
				val_t v = whatever(lex);
				pushkv(&r, k, v);
			}
			must(lex, ">>");
			return r;
		}
		case "[": {
			val_t r = { .type = ARR };
			while (true) {
				if (lpeek(lex, "]")) break;
				pushval(&r, whatever(lex));
			}
			must(lex, "]");
			return r;
		}
		case "id": {
			val_t v = { .type = NAME };
			strcpy(v.chars, tok.content);
			return v;
		}
		case "num": {
			if (strcmp(pdflex.peek(lex, 0).name, "num") == 0 && strcmp(pdflex.peek(lex, 1).name, "R") == 0) {
				int ver = mustint(lex);
				if (ver != 0) {
					panic("expected zero ver, got %d", ver);
				}
				must(lex, "R");
				val_t v = { .type = REF, .num = getint(tok) };
				return v;
			}
			val_t v = { .type = NUM, .num = getint(tok) };
			return v;
		}
		case "hex": {
			if (strcmp(tok.name, "hex") == 0) {
				val_t r = { .type = HEX };
				strcpy(r.chars, tok.content);
				return r;
			}
		}
		case "false": {
			val_t r = { .type = TFALSE };
			return r;
		}
		case "string": {
			val_t r = { .type = STR };
			strcpy(r.chars, tok.content);
			return r;
		}
	}
	panic("got unexpected {%s %s}", tok.name, tok.content);
}

void pushval(val_t *x, item) {
	if (x->size == x->cap) {
		x->cap *= 2;
		if (x->cap == 0) {
			x->cap = 10;
		}
		x->vals = realloc(x->vals, sizeof(val_t) * x->cap);
		if (!x->vals) {
			panic("realloc failed");
		}
	}
	x->vals[x->size++] = item;
}

void pushkv(val_t *x, k, v) {
	if (x->size == x->cap) {
		x->cap *= 2;
		if (x->cap == 0) {
			x->cap = 10;
		}
		x->vals = realloc(x->vals, sizeof(val_t) * x->cap);
		x->keys = realloc(x->keys, sizeof(val_t) * x->cap);
		if (!x->vals || !x->keys) {
			panic("realloc failed");
		}
	}
	x->keys[x->size] = k;
	x->vals[x->size] = v;
	x->size++;
}

int getrefpos(reader_t *p, int id) {
	int pos = -1;
	for (int i = 0; i < p->nxrefs; i++) {
		xref_list_t l = p->xrefs[i];
		for (int j = 0; j < l.n; j++) {
			xref_entry_t entry = l.items[j];
			if (entry.id == id) {
				pos = entry.pos;
				// keep going, may be overridden.
			}
		}
	}
	if (pos == -1) {
		panic("xref not found: %d", id);
	}
	return pos;
}

int getint(pdflex.tok_t tok) {
	int r = 0;
	sscanf(tok.content, "%d", &r);
	return r;
}

int mustint(pdflex.lexer_t *lex) {
	return getint(must(lex, "num"));
}

xref_list_t read_xref(pdflex.lexer_t *lex) {
	must(lex, "xref");
	int objid = mustint(lex);
	int listsize = mustint(lex);

	xref_list_t r = {
		.items = calloc!(listsize, sizeof(xref_entry_t))
	};
	for (int i = 0; i < listsize; i++) {
		xref_entry_t *e = &r.items[r.n++];

		int pos = mustint(lex);
		mustint(lex);
		pdflex.tok_t status = pdflex.get(lex);
		switch str (status.name) {
			case "n": { e->in_use = true; }
			case "f": { e->in_use = false; }
			default: {
				panic("unexpected xref entry status: %s", status.content);
			}
		}
		e->id = objid+i;
		e->pos = pos;
	}
	return r;
}
