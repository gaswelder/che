#import formats/json
#import opt
#import strings
#import tty

char *reqfields[100] = {};
int nreqfields = 0;

int main(int argc, char **argv) {
    char buf[4096];

    char *fields_string = NULL;
    opt.str("f", "comma-separated list of fields to print as columns after the timestamp", &fields_string);
    opt.opt_parse(argc, argv);

    nreqfields = strings.split(",", fields_string, reqfields, sizeof(reqfields));

    // This assumes that stdin has the default "line discipline" - that is,
    // fgets will end up delivering whole lines, one line at a time, - and that
    // all lines fit into the buffer.
    while (fgets(buf, 4096, stdin)) {
        json.val_t *current_object = json.parse(buf);
        // If line couldn't be parsed as json - print it as is.
        if (!current_object) {
            printf("%s", buf);
            continue;
        }
        print_object(current_object);
        json.json_free(current_object);
    }
    return 0;
}

void print_object(json.val_t *current_object) {
    // Print the level in color. If the level field is missing, print a
    // placeholder ("none").
    const char *level = json.json_getstr(current_object, "level");
    if (level == NULL) {
        level = "none";
    }
    if (strcmp(level, "error") == 0) {
        tty.ttycolor(tty.RED);
    } else if (strcmp(level, "info") == 0) {
        tty.ttycolor(tty.BLUE);
    } else {
        tty.ttycolor(tty.YELLOW);
    }
    printf("%s", level);
    tty.ttycolor(tty.RESET_ALL);

    // Print the timestamp or a placeholder.
    const char *ts = json.json_getstr(current_object, "t");
    if (!ts) {
        ts = json.json_getstr(current_object, "timestamp");
    }
    if (!ts) {
        printf("\t(?time)");
    } else {
        printf("\t%s", ts);
    }

    // Print the requested fields.
    for (int i = 0; i < nreqfields; i++) {
        json.val_t *v = json.get(current_object, reqfields[i]);
        if (v != NULL) {
            printf("\t");
            print_node(reqfields[i], current_object, v);
        } else {
            printf("\t(?%s)", reqfields[i]);
        }
    }

    // Print the message.
    const char *msg = json.json_getstr(current_object, "msg");
    if (!msg) {
        msg = json.json_getstr(current_object, "message");
    }
    if (msg != NULL) {
        printf("\t%s", msg);
    } else {
        printf("\t");
    }

    // Print the remaining fields as k=v
    size_t n = json.nkeys(current_object);
    for (size_t i = 0; i < n; i++) {
        const char *key = json.key(current_object, i);

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
        json.val_t *val = json.json_val(current_object, i);
        printf(" %s=", key);
        print_node(key, current_object, val);
    }
    puts("");
}

bool isint(double x) {
	int a = (int) x;
	return (double) a == x;
}

void print_node(const char *key, json.val_t *e, *val) {
    switch (json.type(val)) {
        case json.JSON_STR:  { printf("%s", json.json_getstr(e, key)); }
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