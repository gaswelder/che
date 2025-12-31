#import bits
#import reader

pub typedef {
	bool ischar; // true if this node is a value node, false if a tree.
	int val; // the value this node encodes. zero is valid.
	size_t count; // character count, used when building trees from text.
	node_t *left, *right;
} node_t;

pub typedef {
	node_t *root; // the tree root.
	node_t *pool; // malloc root, for the free call.
	size_t poolpos;
} tree_t;

tree_t *newtree() {
	tree_t *t = calloc!(1, sizeof(tree_t));
	t->pool = calloc!(512, sizeof(node_t));
	t->poolpos = 0;
	return t;
}

node_t *newnode(tree_t *t) {
	if (t->poolpos == 512) {
		panic("too many nodes");
	}
	return &t->pool[t->poolpos++];
}

pub void freetree(tree_t *t) {
	free(t->pool);
	free(t);
}

// Builds a canonical tree from the given length counts and values.
pub tree_t *treefrom(uint8_t *lencounts, size_t lencountslen, uint8_t *vals) {
	tree_t *t = newtree();
	t->root = newnode(t);
	if (!t->root) panic("wtf");
	int c = 0;
	for (uint8_t i = 0; i < lencountslen; i++) {
		uint8_t len = i+1;
		uint8_t count = lencounts[i];
		for (uint8_t j = 0; j < count; j++) {
			uint8_t ch = vals[c++];
			node_t *n = createnode(t, t->root, len);
			if (!n) {
				panic("failed to create a node");
			}
			n->val = ch;
		}
	}
	return t;
}

// Creates a value node at the given depth from the node n.
node_t *createnode(tree_t *t, node_t *n, int depth) {
	if (!n) panic("n is null");
	if (depth == 1) {
		if (!n->left) {
			n->left = newnode(t);
			n->left->ischar = true;
			return n->left;
		}
		if (!n->right) {
			n->right = newnode(t);
			n->right->ischar = true;
			return n->right;
		}
		return NULL;
	}
	if (!n->left) {
		n->left = newnode(t);
	}
	if (!n->left->ischar) {
		node_t *x = createnode(t, n->left, depth-1);
		if (x) return x;
	}
	if (!n->right) {
		n->right = newnode(t);
	}
	if (!n->right->ischar) {
		node_t *x = createnode(t, n->right, depth-1);
		if (x) return x;
	}
	return NULL;
}



// Builds a tree from input text s.
pub tree_t *buildtree(const char *s) {
	// Count each character
	size_t counts[256] = {};
	const char *p = NULL;
	for (p = s; *p != '\0'; p++) {
		uint8_t x = *p;
		counts[x]++;
	}

	tree_t *t = newtree();
	node_t *list[256] = {};
	size_t listlen = 0;
	for (int i = 0; i < 256; i++) {
		if (counts[i] == 0) continue;
		node_t *n = newnode(t);
		n->val = i;
		n->count = counts[i];
		list[listlen++] = n;
	}

	// Brute-force approach
	while (listlen > 1) {
		// Take two rarest a, b
		qsort(list, listlen, sizeof(node_t *), cmp);
		node_t *a = list[listlen-1];
		node_t *b = list[listlen-2];

		// Merge them into a new node
		node_t *sum = newnode(t);
		sum->left = a;
		sum->right = b;
		sum->count = a->count + b->count;
		list[listlen-2] = sum;
		listlen--;
	}
	t->root = list[0];
	return t;
}

int cmp(const void *a, *b) {
	node_t **ca = (node_t **) a;
	node_t **cb = (node_t **) b;
	node_t *x = *ca;
	node_t *y = *cb;
	if (y->count > x->count) return 1;
	if (y->count < x->count) return -1;
	return 0;
}

pub typedef {
	bits.reader_t *br;
	node_t *root;
	node_t *curr;
} reader_t;

// Returns a new reader to decompress from file f.
pub reader_t *newreader(tree_t *tree, FILE *f) {
	reader_t *r = calloc!(1, sizeof(reader_t));
	reader.t *fr = reader.file(f);
	// reader.free(fr);
	r->br = bits.newreader(fr);
	r->root = tree->root;
	r->curr = tree->root;
	return r;
}

// Closes and frees reader r.
pub void closereader(reader_t *r) {
	bits.closereader(r->br);
	free(r);
}

// Reads next character from reader r.
pub int read(reader_t *r) {
	while (true) {
		int bit = bits.read1(r->br);
		if (bit == EOF) {
			if (r->curr != r->root) {
				panic("unexpected end of input");
			}
			return EOF;
		}
		if (bit == 1) {
			r->curr = r->curr->right;
		} else {
			r->curr = r->curr->left;
		}
		if (!r->curr->left) {
			int b = r->curr->val;
			r->curr = r->root;
			return b;
		}
	}
}

pub typedef {
	uint8_t bits[16];
	uint8_t len;
} bitslice_t;

pub typedef {
	tree_t *tree;
	bitslice_t table[256];
	bits.writer_t *w;
} writer_t;

// Returns a new writer into file f.
pub writer_t *newwriter(tree_t *t, FILE *f) {
	writer_t *w = calloc!(1, sizeof(writer_t));
	w->tree = t;
	inittable(w->table, t->root, NULL, 0);
	w->w = bits.newwriter(f);
	return w;
}

void inittable(bitslice_t *table, node_t *n, uint8_t *pref, uint8_t len) {
	if (n->left) {
		uint8_t code[16] = {};
		for (uint8_t i = 0; i < len; i++) {
			code[i] = pref[i];
		}
		code[len] = 0;
		inittable(table, n->left, code, len+1);
		code[len] = 1;
		inittable(table, n->right, code, len+1);
	} else {
		table[n->val].len = len;
		for (uint8_t i = 0; i < len; i++) {
			table[n->val].bits[i] = pref[i];
		}
	}
}

pub void closewriter(writer_t *w) {
	if (!bits.closewriter(w->w)) {
		panic("!");
	}
	free(w);
}

pub bool write(writer_t *w, uint8_t b) {
	bitslice_t *bs = &w->table[b];
	for (uint8_t i = 0; i < bs->len; i++) {
		if (!bits.writebit(w->w, bs->bits[i])) {
			return false;
		}
	}
	return true;
}


typedef {
	char code[20];
	int val;
} print_t;

pub void printtree(tree_t *t) {
	if (!t) panic("t is null");
	printnode(t->root, 0);
}

void printnode(node_t *n, int indent) {
	if (!n) panic("n is null");
	for (int i = 0; i < indent; i++) {
		printf(" . ");
	}
	if (n->ischar) {
		if (isprint(n->val)) {
			printf("val('%c')\n", n->val);
		} else {
			printf("val(0x%x)\n", n->val);
		}
	} else {
		printf("tree:\n");
		if (n->left) printnode(n->left, indent+1);
		if (n->right) printnode(n->right, indent+1);
	}
}

int cmp218(const void *a, *b) {
	const print_t *pa = a;
	const print_t *pb = b;
	return strcmp(pa->code, pb->code);
}

pub void printcodes(tree_t *t) {
	print_t out[100] = {};
	print_t *x = outputnode(t->root, "", out);
	size_t n = x - (print_t *)out;
	if (n > 100) {
		panic("too many print nodes");
	}
	qsort(out, n, sizeof(print_t), cmp218);
	for (size_t i = 0; i < n; i++) {
		print_t *a = &out[i];
		int val = a->val;
		if (isprint(val)) {
			printf("'%c'", val);
		} else {
			printf("0x%x", val);
		}
		printf("\t%s\n", a->code);

	}
}

print_t *outputnode(node_t *n, const char *prefix, print_t *out) {
	if (!n->left && !n->right) {
		strcpy(out->code, prefix);
		out->val = n->val;
		out++;
		return out;
	}
	if (n->left) {
		char buf[20] = {};
		size_t len = strlen(prefix);
		if (len >= 20) {
			panic("prefix too long");
		}
		strcpy(buf, prefix);
		buf[len] = '0';
		out = outputnode(n->left, buf, out);
	}
	if (n->right) {
		char buf[20] = {};
		size_t len = strlen(prefix);
		if (len >= 20) {
			panic("prefix too long");
		}
		strcpy(buf, prefix);
		buf[len] = '1';
		out = outputnode(n->right, buf, out);
	}
	return out;
}
