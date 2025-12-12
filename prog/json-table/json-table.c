#import formats/json

int main() {
	//
	// Read the whole table.
	//
	size_t maxrows = 20000;
	json.val_t **rows = calloc!(maxrows, sizeof(json.val_t *));
	size_t nrows = 0;
	char buf[4096];
	while (fgets(buf, 4096, stdin)) {
		if (nrows == maxrows) {
			fprintf(stderr, "reached max rows limit (%zu)\n", maxrows);
			return 1;
		}
		rows[nrows++] = json.parse(buf);
	}

	//
	// Define the header and calculate the widths.
	//
	const char *keys[100];
	size_t nkeys = json.nkeys(rows[0]);
	for (size_t i = 0; i < nkeys; i++) {
		keys[i] = json.key(rows[0], i);
	}
	size_t colwidth[100] = {};
	for (size_t colid = 0; colid < nkeys; colid++) {
		colwidth[colid] = strlen(keys[colid]);
	}
	for (size_t rowid = 0; rowid < nrows; rowid++) {
		for (size_t colid = 0; colid < nkeys; colid++) {
			const char *key = keys[colid];
			size_t l = json.sprintval(json.get(rows[rowid], key), buf);
			if (l > colwidth[colid]) {
				colwidth[colid] = l;
			}
		}
	}
	size_t w = 0;
	for (size_t i = 0; i < nkeys; i++) {
		w += colwidth[i];
		if (i < nkeys - 1) {
			w += 3;
		}
	}

	// Print the header.
	line(w);
	for (size_t i = 0; i < nkeys; i++) {
		printw(keys[i], colwidth[i]);
		if (i < nkeys-1) {
			printf(" | ");
		}
	}
	putchar('\n');
	line(w);

	// Print the rows.
	for (size_t rowid = 0; rowid < nrows; rowid++) {
		for (size_t colid = 0; colid < nkeys; colid++) {
			const char *key = keys[colid];
			json.sprintval(json.get(rows[rowid], key), buf);
			printw(buf, colwidth[colid]);
			if (colid < nkeys-1) {
				printf(" | ");
			}
		}
		putchar('\n');
	}
	line(w);
	return 0;
}

void printw(const char *s, size_t w) {
	size_t l = strlen(s);
	printf("%s", s);
	while (l < w) {
		putchar(' ');
		l++;
	}
}

void line(size_t w) {
	for (size_t i = 0; i < w; i++) {
		putchar('-');
	}
	putchar('\n');
}
