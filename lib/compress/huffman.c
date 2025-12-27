#import bits

pub typedef {
	int byte;
	size_t count;
	node_t *left, *right;
} node_t;

pub typedef {
	node_t *pool; // malloc root, for the free call.
	node_t *root; // the tree root.
} tree_t;

pub void freetree(tree_t *t) {
	free(t->pool);
	free(t);
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

	tree_t *t = calloc!(1, sizeof(tree_t));
	node_t *pool = calloc!(512, sizeof(node_t));
	size_t poolpos = 0;

	node_t *list[256] = {};
	size_t listlen = 0;
	for (int i = 0; i < 256; i++) {
		if (counts[i] == 0) continue;
		node_t *n = &pool[poolpos++];
		n->byte = i;
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
		node_t *sum = &pool[poolpos++];
		sum->left = a;
		sum->right = b;
		sum->count = a->count + b->count;
		list[listlen-2] = sum;
		listlen--;
	}
	t->pool = pool;
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
	r->br = bits.newreader(f);
	r->root = tree->root;
	r->curr = tree->root;
	return r;
}

// Closes and frees reader r.
pub void closereader(reader_t *r) {
	bits.closereader(r->br);
	free(r);
}

// Reads next byte from reader r.
pub int read(reader_t *r) {
	while (true) {
		int bit = bits.bits_get(r->br);
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
			int b = r->curr->byte;
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
		table[n->byte].len = len;
		for (uint8_t i = 0; i < len; i++) {
			table[n->byte].bits[i] = pref[i];
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

// pub void printtree(tree_t *t) {
// 	printnode(t->root);
// }

// void printnode(node_t *n) {
//	 if (n->left) {
//		 printf("n=%zu [", n->count);
//		 printnode(n->left);
//		 printf("] [");
//		 printnode(n->right);
//		 printf("]");
//	 } else {
//		 printf("n=%zu byte=%d (%c)", n->count, n->byte, n->byte);
//	 }
// }
