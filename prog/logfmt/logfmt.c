#import formats/json
#import opt
#import strings
#import tty

char *reqfields[100] = {};
int nreqfields = 0;

int main(int argc, char **argv) {
    char buf[4096];

    char *fields_string = NULL;
	opt.nargs(0, "");
    opt.str("f", "comma-separated list of fields to print as columns after the timestamp", &fields_string);
    opt.parse(argc, argv);

    nreqfields = strings.split(",", fields_string, reqfields, sizeof(reqfields));

    // This assumes that stdin has the default "line discipline" - that is,
    // fgets will end up delivering whole lines, one line at a time, - and that
    // all lines fit into the buffer.
    while (fgets(buf, 4096, stdin)) {
        json.val_t *entry = json.parse(buf);
        // If line couldn't be parsed as json - print it as is.
        if (!entry) {
            printf("%s", buf);
            continue;
        }
        print_entry(entry);
        json.json_free(entry);
    }
    return 0;
}

void print_entry(json.val_t *entry) {
    print_level(entry);

    // Print the timestamp or a placeholder.
    const char *ts = json.getstr(entry, "t");
    if (!ts) {
        ts = json.getstr(entry, "timestamp");
    }
    if (!ts) {
        printf("\t(?time)");
    } else {
        printf("\t%s", ts);
    }

    // Print the requested fields.
    for (int i = 0; i < nreqfields; i++) {
        json.val_t *v = json.get(entry, reqfields[i]);
		printf("\t");
        if (v != NULL) {
            print_node(reqfields[i], entry, v);
        }
    }

    // Print the message.
    const char *msg = json.getstr(entry, "msg");
    if (!msg) {
        msg = json.getstr(entry, "message");
    }
    if (msg != NULL) {
        printf("\t%s", msg);
    } else {
        printf("\t");
    }

    // Print the remaining fields as k=v
    size_t n = json.nkeys(entry);
    for (size_t i = 0; i < n; i++) {
        const char *key = json.key(entry, i);

        // Skip if we have already printed this field.
        if (!strcmp(key, "t") || !strcmp(key, "msg") || !strcmp(key, "level")) {
            continue;
        }
        bool isreq = false;
        for (int j = 0; j < nreqfields; j++) {
            if (!strcmp(reqfields[j], key)) {
                isreq = true;
                break;
            }
        }
        if (isreq) {
            continue;
        }
        json.val_t *val = json.json_val(entry, i);
        printf(" %s=", key);
        print_node(key, entry, val);
    }
    puts("");
}

// Prints the level in color.
// If the level field is missing, prints a placeholder ("none").
void print_level(json.val_t *entry) {
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
	int a = (int) x;
	return (double) a == x;
}

void print_node(const char *key, json.val_t *e, *val) {
    switch (json.type(val)) {
        case json.JSON_STR:  { printf("%s", json.getstr(e, key)); }
        case json.JSON_OBJ:  { printf("(object)"); }
        case json.JSON_NULL: { printf("null"); }
        case json.JSON_ARR:  { printf("(array)"); }
        case json.JSON_NUM: {
            double val = json.json_getdbl(e, key);
            if (isint(val)) {
                printf("%d", (int) val);
            } else {
                printf("%f", val);
            }
        }
	    case json.JSON_BOOL: {
            if (val->val.boolval) {
                printf("true");
            } else {
                printf("false");
            }
        }
        default: {
            printf("(unknown type %d)", json.type(val));
        }
    }
}