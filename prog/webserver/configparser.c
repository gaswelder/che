#import clip/stringtable
#import strings

#import config.c

#define N_GROW_STRINGLIST 10

typedef {
    LKString **items;
    size_t items_len;
    size_t items_size;
} LKStringList;



// Read config file and set config structure.
//
// Config file format:
// -------------------
//
//    # Matches all other hostnames
//    hostname *
//    homedir=/var/www/testsite
//    alias latest=latest.html
//
//    # Matches http://localhost
//    hostname localhost
//    homedir=/var/www/testsite
//
//    # http://littlekitten.xyz
//    hostname littlekitten.xyz
//    homedir=/var/www/testsite
//    cgidir=cgi-bin
//    alias latest=latest.html
//    alias about=about.html
//    alias guestbook=cgi-bin/guestbook.pl
//    alias blog=cgi-bin/blog.pl
//
//    # http://newsboard.littlekitten.xyz
//    hostname newsboard.littlekitten.xyz
//    proxyhost=localhost:8001
//
// Format description:
// The host and port number is defined first, followed by one or more
// host config sections. The host config section always starts with the
// 'hostname <domain>' line followed by the settings for that hostname.
// The section ends on either EOF or when a new 'hostname <domain>' line
// is read, indicating the start of the next host config section.
//

#define CONFIG_LINE_SIZE 255

enum {CFG_ROOT, CFG_HOSTSECTION};

pub config.LKConfig *read_file(const char *configfile) {
    FILE *f = fopen(configfile, "r");
    if (f == NULL) {
        return NULL;
    }

    char line[CONFIG_LINE_SIZE];
    int state = CFG_ROOT;
    config.LKConfig *cfg = config.lk_config_new();
    LKString *l = lk_string_new("");
    LKString *k = lk_string_new("");
    LKString *v = lk_string_new("");
    LKString *aliask = lk_string_new("");
    LKString *aliasv = lk_string_new("");
    config.LKHostConfig *hc = NULL;

    while (1) {
        char *pz = fgets(line, sizeof(line), f);
        if (pz == NULL) {
            break;
        }
        printf("state = %d, line: %s", state, line);
        lk_string_assign(l, line);
        lk_string_trim(l);

        // Skip # comment line.
        if (lk_string_starts_with(l, "#")) {
            printf("skipping comment\n");
            continue;
        }

        // hostname littlekitten.xyz
        lk_string_split_assign(l, " ", k, v); // l:"k v", assign k and v
        printf("k='%s'\n", k->s);

        if (lk_string_sz_equal(k, "hostname")) {
            // hostname littlekitten.xyz
            hc = config.lk_config_create_get_hostconfig(cfg, v->s);
            state = CFG_HOSTSECTION;
            continue;
        }

        if (state == CFG_ROOT) {
            // serverhost=127.0.0.1
            // port=8000
            lk_string_split_assign(l, "=", k, v); // l:"k=v", assign k and v
            if (lk_string_sz_equal(k, "serverhost")) {
                panic("unknown clause: serverhost");
                continue;
            } else if (lk_string_sz_equal(k, "port")) {
                panic("unknown clause: port");
                continue;
            }
            continue;
        }
        if (state == CFG_HOSTSECTION) {
            assert(hc != NULL);

            // homedir=testsite
            // cgidir=cgi-bin
            // proxyhost=localhost:8001
            lk_string_split_assign(l, "=", k, v);
            if (lk_string_sz_equal(k, "homedir")) {
                strcpy(hc->homedir, v->s);
                continue;
            } else if (lk_string_sz_equal(k, "cgidir")) {
                strcpy(hc->cgidir, v->s);
                continue;
            } else if (lk_string_sz_equal(k, "proxyhost")) {
                strcpy(hc->proxyhost, v->s);
                continue;
            }
            // alias latest=latest.html
            lk_string_split_assign(l, " ", k, v);
            if (lk_string_sz_equal(k, "alias")) {
                lk_string_split_assign(v, "=", aliask, aliasv);
                if (!lk_string_starts_with(aliask, "/")) {
                    lk_string_prepend(aliask, "/");
                }
                if (!lk_string_starts_with(aliasv, "/")) {
                    lk_string_prepend(aliasv, "/");
                }
                stringtable.set(hc->aliases, aliask->s, aliasv->s);
                continue;
            }
            continue;
        }
    }

    lk_string_free(l);
    lk_string_free(k);
    lk_string_free(v);
    lk_string_free(aliask);
    lk_string_free(aliasv);

    fclose(f);
    return cfg;
}

// Given a "k<delim>v" string, assign k and v.
void lk_string_split_assign(LKString *lks, const char *delim, LKString *k, *v) {
    char *result[2] = {NULL};

    size_t n = strings.split(delim, lks->s, result, nelem(result));
    printf("split = %zu\n", n);
    lk_string_assign(k, result[0]);
    free(result[0]);
    if (n == 2) {
        lk_string_assign(v, result[1]);
        free(result[1]);
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


int lk_string_sz_equal(LKString *lks1, const char *s2) {
    if (!strcmp(lks1->s, s2)) {
        return 1;
    }
    return 0;
}

// Return if string starts with s.
int lk_string_starts_with(LKString *lks, const char *s) {
    return strings.starts_with(lks->s, s);
}

// Remove leading and trailing white from string.
void lk_string_trim(LKString *lks) {
    char *copy = strings.newstr("%s", lks->s);
    char *r = strings.trim(copy);
    lk_string_assign(lks, r);
    free(copy);
}
