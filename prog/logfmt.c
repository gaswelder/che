#import json
#import tty
#import opt

int main(int argc, char **argv) {
    char buf[4096];

    char *fields_string = NULL;
    opt.str("f", "fields to print as columns after the timestamp", &fields_string);
    opt.parse(argc, argv);

    int nreqfields = 0;
    char *reqfields[100];

    if (fields_string) {
        char *p = fields_string;
        char *a = fields_string;
        while (true) {
            if (*p == ',' || *p == '\0') {
                char *m = calloc(p - a + 1, 1);
                int i = 0;
                while (a != p) {
                    m[i++] = *a;
                    a++;
                }
                reqfields[nreqfields++] = m;
                a = p+1;
            }
            if (*p == '\0') {
                break;
            }
            p++;
        }
    }

    // This assumes that stdin has the default "line discipline" - that is,
    // fgets will end up delivering whole lines, one line at a time, - and that
    // all lines fit into the buffer.
    while (fgets(buf, 4096, stdin)) {
        json_node *e = json_parse(buf);
        // If line couldn't be parsed as json - print it as is.
        if (!e) {
            printf("%s", buf);
            continue;
        }

        // Print the level in color. If the level field is missing, print a
        // placeholder ("none").
        const char *level = json_getstr(e, "level");
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
        const char *ts = json_getstr(e, "t");
        if (ts == NULL) {
            printf("\t(?time)");
        } else {
            printf("\t%s", ts);
        }

        // Print the requested fields.
        for (int i = 0; i < nreqfields; i++) {
            json_node *v = json_get(e, reqfields[i]);
            if (v != NULL) {
                printf("\t");
                print_node(reqfields[i], e, v);
            } else {
                printf("\t(?%s)", reqfields[i]);
            }
        }

        // Print the message.
        const char *msg = json_getstr(e, "msg");
        if (msg != NULL) {
            printf("\t%s", msg);
        } else {
            printf("\t");
        }

        // Print the remaining fields as k=v
        size_t n = json_size(e);
        for (size_t i = 0; i < n; i++) {
            const char *key = json_key(e, i);

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
            json_node *val = json_val(e, i);
            printf(" %s=", key);
            print_node(key, e, val);
        }
        puts("");
    }
    return 0;
}

void print_node(const char *key, json_node *e, *val) {
    switch (json_type(val)) {
        case JSON_STR:
            printf("%s", json_getstr(e, key));
            break;
        case JSON_NUM:
            double val = json_getdbl(e, key);
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
            printf("unknown type: %d\n", json_type(val));
    }
}