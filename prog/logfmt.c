#import json
#import tty

int main() {
    char buf[4096];

    while (fgets(buf, 4096, stdin)) {
        json_node *e = json_parse(buf);
        if (!e) {
            printf("%s", buf);
            continue;
        }

        const char *level = json_getstr(e, "level");
        if (strcmp(level, "error") == 0) {
            ttycolor(RED);
        } else if (strcmp(level, "info") == 0) {
            ttycolor(BLUE);
        } else {
            ttycolor(YELLOW);
        }
        printf("%s", level);
        ttycolor(RESET_ALL);
        

        const char *ts = json_getstr(e, "t");
        printf("\t%s\t%s", ts, json_getstr(e, "msg"));

        size_t n = json_size(e);
        for (size_t i = 0; i < n; i++) {
            const char *key = json_key(e, i);
            if (!strcmp(key, "t") || !strcmp(key, "msg") || !strcmp(key, "level")) {
                continue;
            }
            
            json_node *val = json_val(e, i);
            printf(" %s=", key);
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
        puts("");
    }
    return 0;
}
