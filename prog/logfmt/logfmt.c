#import formats/json
#import opt
#import strings
#import tty

char *reqfields[100] = {};
char *excludefields[100] = {};
int nreqfields = 0;
int nexclude = 0;

int main(int argc, char **argv) {
    char buf[4096];

    char *fields_string = "msg|message";
    char *exclude_string = "";
	opt.nargs(0, "");
    opt.str("f", "comma-separated list of fields to print as columns after the level and timestamp", &fields_string);
    opt.str("x", "comma-separated list of fields to exclude", &exclude_string);
    opt.parse(argc, argv);

    nreqfields = strings.split(",", fields_string, reqfields, sizeof(reqfields));
    nexclude = strings.split(",", exclude_string, excludefields, sizeof(excludefields));

    // This assumes that stdin has the default "line discipline" - that is,
    // fgets will deliver complete lines, one at a time, and that all lines fit into the buffer.
    while (fgets(buf, sizeof(buf), stdin)) {
        json.val_t *entry = json.parse(buf);
        // If line couldn't be parsed as json, print as is.
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
    char printed[10][100] = {
        "level",
    };
    size_t nprinted = 1;

    print_level(entry);
    putchar('\t');
    if (print_req_val(entry, "t|timestamp", printed[nprinted])) {
        nprinted++;
    }
    putchar('\t');

    for (int i = 0; i < nreqfields; i++) {
        if (print_req_val(entry, reqfields[i], printed[nprinted])) {
            nprinted++;
        }
        putchar('\t');
    }

    // Print the remaining fields as k=v
    size_t n = json.nkeys(entry);
    for (size_t i = 0; i < n; i++) {
        const char *key = json.key(entry, i);
        if (contains(printed, key)) {
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

bool print_req_val(json.val_t *entry, const char *key, char printed[100]) {
    if (strcmp(key, "msg|message") == 0) {
        return print_req_val(entry, "msg", printed) || print_req_val(entry, "message", printed);
    }
    if (strcmp(key, "t|timestamp") == 0) {
        return print_req_val(entry, "t", printed) || print_req_val(entry, "timestamp", printed);
    }
    json.val_t *v = json.get(entry, key);
    if (v == NULL) {
        return false;
    }
    strcpy(printed, key);
    printval(v);
    return true;
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

void printval(json.val_t *val) {
    switch (val->type) {
        case json.JSON_STR: { printf("%s", val->val.str); }
        case json.JSON_OBJ: { printf("(object)"); }
        case json.JSON_NULL: { printf("null"); }
        case json.JSON_ARR: { printf("(array)"); }
        case json.JSON_NUM: { printf("%g", val->val.num); }
        case json.JSON_BOOL: {
            if (val->val.boolval) {
                printf("true");
            } else {
                printf("false");
            }
        }
        default: { printf("(unimplemented type %d)", val->type); }
    }
}

bool contains(const char ids[][100], *id) {
    size_t i = 0;
    for (;;) {
        if (ids[i][0] == '\0') break;
        if (strcmp(ids[i], id) == 0) return true;
        i++;
    }
    return false;
}
