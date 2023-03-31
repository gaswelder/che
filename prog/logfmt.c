#import json
#import tty
#import opt
#import strutil

int main(int argc, char **argv) {
    char buf[4096];

    char *fields_string = NULL;
    opt.str("f", "fields to print as columns after the timestamp", &fields_string);
    opt.parse(argc, argv);

    char *reqfields[100];
    int nreqfields = strutil.split(',', fields_string, reqfields, 100);

    // This assumes that stdin has the default "line discipline" - that is,
    // fgets will end up delivering whole lines, one line at a time, - and that
    // all lines fit into the buffer.
    while (fgets(buf, 4096, stdin)) {
        json.node *current_object = json.parse(buf);
        // If line couldn't be parsed as json - print it as is.
        if (!current_object) {
            printf("%s", buf);
            continue;
        }

        // Print the level in color. If the level field is missing, print a
        // placeholder ("none").
        const char *level = json.getstr(current_object, "level");
        if (level == NULL) {
            level = "none";
        }
        if (strcmp(level, "error") == 0) {
            ttycolor(RED);
        } else if (strcmp(level, "info") == 0) {
            ttycolor(BLUE);
        } else {
            ttycolor(YELLOW);
        }
        printf("%s", level);
        ttycolor(RESET_ALL);

        // Print the timestamp or a placeholder.
        const char *ts = json.getstr(current_object, "t");
        if (ts == NULL) {
            printf("\t(?time)");
        } else {
            printf("\t%s", ts);
        }

        // Print the requested fields.
        for (int i = 0; i < nreqfields; i++) {
            json.node *v = json.get(current_object, reqfields[i]);
            if (v != NULL) {
                printf("\t");
                print_node(reqfields[i], current_object, v);
            } else {
                printf("\t(?%s)", reqfields[i]);
            }
        }

        // Print the message.
        const char *msg = json.getstr(current_object, "msg");
        if (msg != NULL) {
            printf("\t%s", msg);
        } else {
            printf("\t");
        }

        // Print the remaining fields as k=v
        size_t n = json.size(current_object);
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
            json.node *val = json.val(current_object, i);
            printf(" %s=", key);
            print_node(key, current_object, val);
        }
        puts("");
    }
    return 0;
}

void print_node(const char *key, json_node *e, *val) {
    switch (json.type(val)) {
        case JSON_STR:
            printf("%s", json.getstr(e, key));
            break;
        case JSON_NUM:
            double val = json.getdbl(e, key);
            if (round(val) - val < 1e-10) {
                printf("%d", (int) val);
            } else {
                printf("%f", val);
            }
            break;
        case JSON_OBJ:
            printf("(object)");
            break;
        default:
            printf("unknown type: %d\n", json.type(val));
    }
}