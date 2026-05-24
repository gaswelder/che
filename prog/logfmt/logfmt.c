#import formats/json
#import linereader
#import opt
#import strings
#import tty

char *excludefields[100] = {};
int nexclude = 0;

int main(int argc, char **argv) {
    char *exclude_string = "";
	opt.nargs(0, "");
    opt.str("x", "comma-separated list of fields to exclude", &exclude_string);
    opt.parse(argc, argv);

    nexclude = strings.split(",", exclude_string, excludefields, sizeof(excludefields));

    linereader.t *lr = linereader.new(stdin);
	json.err_t err = {};
    while (linereader.read(lr)) {
		char *line = linereader.line(lr);
        json.val_t *entry = json.parse(line, &err);
		// If line couldn't be parsed as json, print as is.
		if (err.set) {
			err.set = false;
			printf("%s", line);
            continue;
		}
        print_entry(entry);
        json.json_free(entry);
    }
	linereader.free(lr);
    return 0;
}

void print_entry(json.val_t *entry) {
    printlevel(entry);
	putchar(' ');

	printtime(entry);
    putchar(' ');

	printmsg(entry);
	putchar(' ');

	printfields(entry);
    puts("");
}

void printfields(json.val_t *entry) {
	tty.ttycolor(tty.DIM);
    size_t n = json.len(entry);
    for (size_t i = 0; i < n; i++) {
        const char *key = json.key(entry, i);
		if (strcmp(key, "level") == 0
			|| strcmp(key, "t") == 0
			|| strcmp(key, "timestamp") == 0
			|| strcmp(key, "msg") == 0
			|| strcmp(key, "message") == 0) {
			continue;
		}
        bool excluded = false;
        for (int j = 0; j < nexclude; j++) {
            if (strcmp(excludefields[j], key) == 0) {
                excluded = true;
                break;
            }
        }
        if (excluded) continue;
        printf(" %s=", key);
        printval(json.val(entry, i));
    }
	tty.ttycolor(tty.RESET_ALL);
}

void printmsg(json.val_t *entry) {
	tty.ttycolor(tty.BRIGHT);
	if (!printfield(entry, "msg")) {
		printfield(entry, "message");
	}
	tty.ttycolor(tty.RESET_ALL);
}

bool printfield(json.val_t *entry, const char *key) {
    json.val_t *v = json.get(entry, key);
    if (v == NULL) {
        return false;
    }
    printval(v);
    return true;
}

// Prints the level in color.
// If the level field is missing, prints a placeholder ("none").
void printlevel(json.val_t *entry) {
	const char *level = json.strval(json.get(entry, "level"));
    if (level == NULL) {
		printf("%s", "none");
		return;
    }
	switch str (level) {
		case "error", "ERROR": { tty.ttycolor(tty.RED); }
		case "info", "INFO": { tty.ttycolor(tty.BLUE); }
		default: { tty.ttycolor(tty.YELLOW); }
	}

	// Print the level in lower case.
	const char *p = level;
	while (*p != '\0') {
		putchar(tolower(*p));
		p++;
	}
    tty.ttycolor(tty.RESET_ALL);
}

void printtime(json.val_t *entry) {
	tty.ttycolor(tty.DIM);
    if (!printfield(entry, "t")) {
		printfield(entry, "timestamp");
	}
	tty.ttycolor(tty.RESET_ALL);
}

bool isint(double x) {
	return fabs(x) <= 9007199254740991.0 && fabs(x - round(x)) == 0.0;
}

void printval(json.val_t *val) {
    switch (val->type) {
        case json.TSTR: { printf("%s", val->val.str); }
        case json.TOBJ: { printf("(object)"); }
        case json.TNULL: { printf("null"); }
        case json.TARR: { printf("(array)"); }
        case json.TNUM: {
			double x = val->val.num;
			if (isint(x)) {
				printf("%.0f", x);
			} else {
				printf("%.17g", x);
			}
		}
        case json.TBOOL: {
            if (val->val.boolval) {
                printf("true");
            } else {
                printf("false");
            }
        }
        default: { printf("(unimplemented type %d)", val->type); }
    }
}
