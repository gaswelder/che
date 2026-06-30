#import clip/vec
#import error
#import formats/json

char buf[4096] = {};
bool haveline = true;
bool loadline() {
	haveline = fgets(buf, 4096, stdin) != NULL;
	return haveline;
}

int main() {
	loadline();
	while (haveline) {
		if (buf[0] == '{' && maybeTable()) {
			continue;
		}
		if (buf[0] == '[' && maybeInlineTable()) {
			continue;
		}
		printf("%s", buf);
		loadline();
	}
	return 0;
}

bool maybeTable() {
	vec.t *rows = vec.new(sizeof(json.val_t *));
	bool ok = false;
	while (true) {
		error.t err = {};
		json.val_t *line = json.parse(buf, &err);
		if (err.set) {
			break;
		}
		vec.push(rows, &line);
		ok = true;
		if (!loadline()) {
			break;
		}
	}
	if (ok) printtable(rows);
	freetable(rows);
	return ok;
}

bool maybeInlineTable() {
	// Parse the json array.
	error.t err = {};
	json.val_t *list = json.parse(buf, &err);
	if (err.set) {
		return false;
	}

	// Repack the json array into a list of json objects.
	bool ok = true;
	vec.t *rows = vec.new(sizeof(json.val_t *));
	for (size_t i = 0; i < json.len(list); i++) {
		json.val_t *item = json.val(list, i);
		// We assume it's a table if all items inside are objects.
		if (json.type(item) != json.TOBJ) {
			ok = false;
			break;
		}
		json.val_t *copy = json.clone(item);
		vec.push(rows, &copy);
	}
	json.json_free(list);
	if (!ok) {
		freetable(rows);
		return false;
	}
	printtable(rows);
	freetable(rows);
	return loadline();
}

void freetable(vec.t *rows) {
	size_t n = vec.len(rows);
	for (size_t i = 0; i < n; i++) {
		json.val_t **row = vec.index(rows, i);
		json.json_free(*row);
	}
}

json.val_t *longest(vec.t *rows) {
	size_t curr = 0;
	json.val_t *r = NULL;

	size_t n = vec.len(rows);
	for (size_t i = 0; i < n; i++) {
		json.val_t **row = vec.index(rows, i);
		if (r == NULL || json.len(*row) > curr) {
			curr = json.len(*row);
			r = *row;
		}
	}
	return r;
}

void printtable(vec.t *rows) {
	char buf[4096] = {};
	size_t nrows = vec.len(rows);
	if (nrows == 0) {
		return;
	}

	//
	// Define the header and calculate the widths.
	//
	const char *keys[100];

	// As a quirk, use the row with the most columns
	// to get the columns.
	json.val_t *longestrow = longest(rows);
	size_t nkeys = json.len(longestrow);
	for (size_t i = 0; i < nkeys; i++) {
		keys[i] = json.key(longestrow, i);
	}
	size_t colwidth[100] = {};
	for (size_t colid = 0; colid < nkeys; colid++) {
		colwidth[colid] = strlen(keys[colid]);
	}
	for (size_t rowid = 0; rowid < nrows; rowid++) {
		json.val_t **row = vec.index(rows, rowid);
		for (size_t colid = 0; colid < nkeys; colid++) {
			const char *key = keys[colid];
			json.val_t *col = json.get(*row, key);
			size_t l = sprintval(col, buf);
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

	
	printf("%zu rows\n", nrows);

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
		json.val_t **row = vec.index(rows, rowid);
		for (size_t colid = 0; colid < nkeys; colid++) {
			const char *key = keys[colid];
			sprintval(json.get(*row, key), buf);
			printw(buf, colwidth[colid]);
			if (colid < nkeys-1) {
				printf(" | ");
			}
		}
		putchar('\n');
	}
	line(w);
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

// Writes a string representation of val into str.
int sprintval(json.val_t *val, char *str) {
	if (!val) {
		str[0] = '\0';
		return 0;
	}
	// Print the raw string to avoid quoting and escaping.
	if (json.type(val) == json.TSTR) {
		const char *s = json.strval(val);
		int r = strlen(s);
		strcpy(str, s);
		return r;
	}
	if (json.type(val) == json.TNUM) {
		double x = json.numval(val);
		if (isint(x) && x > 10000) {
			char tmp[20] = {};
			thousands((int) x, tmp, sizeof(tmp));
			strcpy(str, tmp);
			return strlen(tmp);
		}
	}
	// Print other types as serialized JSON.
	char *s = json.format(val);
	int r = strlen(s);
	strcpy(str, s);
	free(s);
	return r;
}

int thousands(int x, char *out, size_t n) {
	char tmp[20];
	int l = sprintf(tmp, "%d", x);

	int extra = l % 3;
	size_t z = 0;
	for (int i = 0; i < l; i++) {
		if (i > 0 && (i-extra) % 3 == 0) {
			if (z == n) return -1;
			out[z++] = '`';
		}
		if (z == n) return -1;
		out[z++] = tmp[i];
	}
	if (z == n) return -1;
	out[z] = '\0';
	return (int) z;
}

bool isint(double x) {
	return x == round(x);
}
