#import formats/json
#import strings

char *header[100] = {};
size_t nheader = 0;

int main() {
    json.val_t *obj = readobj();
    if (!obj) {
        fprintf(stderr, "no objects read\n");
        return 1;
    }

	size_t n = json.nkeys(obj);
	for (size_t i = 0; i < n; i++) {
        if (nheader == 100) {
            fprintf(stderr, "reached max header size (100)\n");
            return 1;
        }
        header[nheader++] = strings.newstr("%s", json.key(obj, i));
    }

    while (obj) {
        printobj(obj);
        obj = readobj();
    }
    return 0;
}

json.val_t *readobj() {
    char line[4096] = {};
    while (fgets(line, sizeof(line), stdin)) {
        json.val_t *obj = json.parse(line);
        if (obj) return obj;
        fprintf(stderr, "failed to parse line as json: \"%s\"\n", line);
    }
    return NULL;
}

void printobj(json.val_t *obj) {
    for (size_t i = 0; i < nheader; i++) {
		if (i > 0) putchar(',');
        json.val_t *v = json.get(obj, header[i]);
		switch (json.type(v)) {
			case json.JSON_NULL: { emit(""); }
			case json.JSON_STR: { emit(json.json_str(v)); }
			default: {
				char *s = json.format(v);
				emit(s);
				free(s);
			}
		}
    }
	putchar('\n');
    json.json_free(obj);
}

void emit(const char *x) {
	putchar('"');
	while (*x) {
		if (*x == '"') putchar('\\');
		putchar(*x);
		x++;
	}
	putchar('"');	
}
