#import formats/json
#import linereader
#import opt
#import strings
#import tty

char *reqfields[100] = {};
char *excludefields[100] = {};
int nreqfields = 0;
int nexclude = 0;

int main(int argc, char **argv) {
    char *fields_string = "msg|message";
    char *exclude_string = "";
	opt.nargs(0, "");
    opt.str("f", "comma-separated list of fields to print as columns after the level and timestamp", &fields_string);
    opt.str("x", "comma-separated list of fields to exclude", &exclude_string);
    opt.parse(argc, argv);

    nreqfields = strings.split(",", fields_string, reqfields, sizeof(reqfields));
    nexclude = strings.split(",", exclude_string, excludefields, sizeof(excludefields));

    linereader.t *lr = linereader.new(stdin);
    while (linereader.read(lr)) {
		char *line = linereader.line(lr);
        json.val_t *entry = json.parse(line);
        // If line couldn't be parsed as json, print as is.
        if (!entry) {
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
	putchar('\t');

    if (!printfield(entry, "t")) {
		printfield(entry, "timestamp");
	}
    putchar('\t');

	if (!printfield(entry, "msg")) {
		printfield(entry, "message");
	}
	putchar('\t');

    // Print the remaining fields as k=v
    size_t n = json.nkeys(entry);
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
        printval(json.json_val(entry, i));
    }
    puts("");
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
	const char *level = json.getstr(entry, "level");
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
