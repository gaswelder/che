#import clip/stringtable
#import strings

#import config.c

#define N_GROW_STRINGLIST 10

typedef {
    LKString **items;
    size_t items_len;
    size_t items_size;
} LKStringList;

#define CONFIG_LINE_SIZE 255

bool unget = false;

char *load_line(FILE *f, char *buf, size_t n) {
    if (unget) {
        char *line = strings.trim(buf);
        unget = false;
        return line;
    }

    while (true) {
        if (!fgets(buf, n, f)) {
            return NULL;
        }
        char *line = strings.trim(buf);

        // Skip empty lines and comments.
        if (!line[0] || line[0] == '#') {
            continue;
        }
        return line;
    }
}

pub config.LKConfig *read_file(const char *configfile) {
    FILE *f = fopen(configfile, "r");
    if (f == NULL) {
        return NULL;
    }
    config.LKConfig *cfg = config.lk_config_new();
    char buf[CONFIG_LINE_SIZE] = {0};
    while (1) {
        config.LKHostConfig *hc = read_hostconfig(f, buf, sizeof(buf));
        if (!hc) {
            break;
        }
        config.add_hostconfig(cfg, hc);
    }
    fclose(f);
    return cfg;
}

config.LKHostConfig *read_hostconfig(FILE *f, char *buf, size_t bufsize) {
    config.LKHostConfig *hc = NULL;
    LKString *l = lk_string_new("");
    LKString *k = lk_string_new("");
    LKString *v = lk_string_new("");
    LKString *aliask = lk_string_new("");
    LKString *aliasv = lk_string_new("");

    // hostname littlekitten.xyz
    char *line = load_line(f, buf, bufsize);
    if (!line) {
        return NULL;
    }
    lk_string_assign(l, line);
    lk_string_split_assign(line, " ", k, v);
    if (strcmp(k->s, "hostname")) {
        panic("expected hostname, got '%s'", k->s);
    }
    hc = config.lk_hostconfig_new(v->s);

    while (true) {
        // homedir=testsite
        // cgidir=cgi-bin
        // proxyhost=localhost:8001
        char *line = load_line(f, buf, bufsize);
        if (!line) {
            break;
        }
        lk_string_assign(l, line);
        lk_string_split_assign(line, "=", k, v);
        if (!strcmp(k->s, "homedir")) {
            strcpy(hc->homedir, v->s);
            continue;
        } else if (!strcmp(k->s, "cgidir")) {
            strcpy(hc->cgidir, v->s);
            continue;
        } else if (!strcmp(k->s, "proxyhost")) {
            strcpy(hc->proxyhost, v->s);
            continue;
        }

        // alias latest=latest.html
        lk_string_split_assign(line, " ", k, v);
        if (!strcmp(k->s, "alias")) {
            lk_string_split_assign(v->s, "=", aliask, aliasv);
            if (!strings.starts_with(aliask->s, "/")) {
                lk_string_prepend(aliask, "/");
            }
            if (!strings.starts_with(aliasv->s, "/")) {
                lk_string_prepend(aliasv, "/");
            }
            stringtable.set(hc->aliases, aliask->s, aliasv->s);
            continue;
        }
        // put the line back
        unget = true;
        break;
    }
    lk_string_free(l);
    lk_string_free(k);
    lk_string_free(v);
    lk_string_free(aliask);
    lk_string_free(aliasv);
    return hc;
}

// Given a "k<delim>v" string, assign k and v.
void lk_string_split_assign(const char *lks, const char *delim, LKString *k, *v) {
    char *splitresult[2] = {NULL};

    size_t n = strings.split(delim, lks, splitresult, nelem(splitresult));
    lk_string_assign(k, splitresult[0]);
    free(splitresult[0]);
    if (n == 2) {
        lk_string_assign(v, splitresult[1]);
        free(splitresult[1]);
    } else {
        lk_string_assign(v, "");
    }
}

typedef {
    char *s;
    size_t s_len;
    size_t s_size;
} LKString;

// Zero out entire size
void zero_s(LKString *lks) {
    memset(lks->s, 0, lks->s_size+1);
}

// Zero out the free unused space
void zero_unused_s(LKString *lks) {
    memset(lks->s + lks->s_len, '\0', lks->s_size+1 - lks->s_len);
}

LKString *lk_string_new(const char *s) {
    if (s == NULL) {
        s = "";
    }
    size_t s_len = strlen(s);

    LKString *lks = calloc(1, sizeof(LKString));
    lks->s_len = s_len;
    lks->s_size = s_len;
    lks->s = calloc(1, lks->s_size+1);
    zero_s(lks);
    strncpy(lks->s, s, s_len);

    return lks;
}

void lk_string_free(LKString *lks) {
    assert(lks->s != NULL);

    zero_s(lks);
    free(lks->s);
    lks->s = NULL;
    free(lks);
}

void lk_string_assign(LKString *lks, const char *s) {
    size_t s_len = strlen(s);
    if (s_len > lks->s_size) {
        lks->s_size = s_len;
        lks->s = realloc(lks->s, lks->s_size+1);
    }
    zero_s(lks);
    strncpy(lks->s, s, s_len);
    lks->s_len = s_len;
}

void lk_string_prepend(LKString *lks, const char *s) {
    size_t s_len = strlen(s);
    if (lks->s_len + s_len > lks->s_size) {
        lks->s_size = lks->s_len + s_len;
        lks->s = realloc(lks->s, lks->s_size+1);
        zero_unused_s(lks);
    }

    memmove(lks->s + s_len, lks->s, lks->s_len); // shift string to right
    strncpy(lks->s, s, s_len);                   // prepend s to string
    lks->s_len = lks->s_len + s_len;
}
