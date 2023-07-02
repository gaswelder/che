#import strings

#import lkconfig.c
#import lkhostconfig.c
#import lkstring.c
#import lkstringtable.c

#define N_GROW_STRINGLIST 10

typedef {
    lkstring.LKString **items;
    size_t items_len;
    size_t items_size;
} LKStringList;



// Read config file and set config structure.
//
// Config file format:
// -------------------
//    serverhost=127.0.0.1
//    port=5000
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

pub lkconfig.LKConfig *read_file(const char *configfile) {
    FILE *f = fopen(configfile, "r");
    if (f == NULL) {
        return NULL;
    }

    char line[CONFIG_LINE_SIZE];
    int state = CFG_ROOT;
    lkconfig.LKConfig *cfg = lkconfig.lk_config_new();
    lkstring.LKString *l = lkstring.lk_string_new("");
    lkstring.LKString *k = lkstring.lk_string_new("");
    lkstring.LKString *v = lkstring.lk_string_new("");
    lkstring.LKString *aliask = lkstring.lk_string_new("");
    lkstring.LKString *aliasv = lkstring.lk_string_new("");
    lkhostconfig.LKHostConfig *hc = NULL;

    while (1) {
        char *pz = fgets(line, sizeof(line), f);
        if (pz == NULL) {
            break;
        }
        lkstring.lk_string_assign(l, line);
        lkstring.lk_string_trim(l);

        // Skip # comment line.
        if (lkstring.lk_string_starts_with(l, "#")) {
            continue;
        }

        // hostname littlekitten.xyz
        lk_string_split_assign(l, " ", k, v); // l:"k v", assign k and v
        if (lkstring.lk_string_sz_equal(k, "hostname")) {
            // hostname littlekitten.xyz
            hc = lkconfig.lk_config_create_get_hostconfig(cfg, v->s);
            state = CFG_HOSTSECTION;
            continue;
        }

        if (state == CFG_ROOT) {
            assert(hc == NULL);

            // serverhost=127.0.0.1
            // port=8000
            lk_string_split_assign(l, "=", k, v); // l:"k=v", assign k and v
            if (lkstring.lk_string_sz_equal(k, "serverhost")) {
                free(cfg->serverhost);
                cfg->serverhost = strings.newstr("%s", v->s);
                continue;
            } else if (lkstring.lk_string_sz_equal(k, "port")) {
                free(cfg->port);
                cfg->port = strings.newstr("%s", v->s);
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
            if (lkstring.lk_string_sz_equal(k, "homedir")) {
                strcpy(hc->homedir, v->s);
                continue;
            } else if (lkstring.lk_string_sz_equal(k, "cgidir")) {
                strcpy(hc->cgidir, v->s);
                continue;
            } else if (lkstring.lk_string_sz_equal(k, "proxyhost")) {
                strcpy(hc->proxyhost, v->s);
                continue;
            }
            // alias latest=latest.html
            lk_string_split_assign(l, " ", k, v);
            if (lkstring.lk_string_sz_equal(k, "alias")) {
                lk_string_split_assign(v, "=", aliask, aliasv);
                if (!lkstring.lk_string_starts_with(aliask, "/")) {
                    lkstring.lk_string_prepend(aliask, "/");
                }
                if (!lkstring.lk_string_starts_with(aliasv, "/")) {
                    lkstring.lk_string_prepend(aliasv, "/");
                }
                lkstringtable.lk_stringtable_set(hc->aliases, aliask->s, aliasv->s);
                continue;
            }
            continue;
        }
    }

    lkstring.lk_string_free(l);
    lkstring.lk_string_free(k);
    lkstring.lk_string_free(v);
    lkstring.lk_string_free(aliask);
    lkstring.lk_string_free(aliasv);

    fclose(f);
    return cfg;
}

LKStringList *lk_stringlist_new() {
    LKStringList *sl = calloc(1, sizeof(LKStringList));
    sl->items_size = N_GROW_STRINGLIST;
    sl->items_len = 0;
    sl->items = calloc(sl->items_size, sizeof(lkstring.LKString));
    return sl;
}

// Given a "k<delim>v" string, assign k and v.
void lk_string_split_assign(lkstring.LKString *lks, const char *delim, lkstring.LKString *k, *v) {
    LKStringList *sl = lk_stringlist_new();
    size_t delim_len = strlen(delim);
    lkstring.LKString *segment = lkstring.lk_string_new("");
    for (size_t i = 0; i < lks->s_len; i++) {
        if (!strncmp(lks->s, delim, delim_len)) {
            lk_stringlist_append_lkstring(sl, segment);
            segment = lkstring.lk_string_new("");
            i += delim_len-1; // need to -1 to account for for loop incrementor i++
            continue;
        }

        lkstring.lk_string_append_char(segment, lks->s[i]);
    }
    lk_stringlist_append_lkstring(sl, segment);

    LKStringList *ss = sl;
    if (k != NULL) {
        lkstring.lk_string_assign(k, ss->items[0]->s);
    }
    if (v != NULL) {
        if (ss->items_len >= 2) {
            lkstring.lk_string_assign(v, ss->items[1]->s);
        } else {
            lkstring.lk_string_assign(v, "");
        }
    }
    lk_stringlist_free(ss);
}

void lk_stringlist_free(LKStringList *sl) {
    assert(sl->items != NULL);

    for (size_t i = 0; i < sl->items_len; i++) {
        lkstring.lk_string_free(sl->items[i]);
    }
    memset(sl->items, 0, sl->items_size * sizeof(lkstring.LKString));

    free(sl->items);
    sl->items = NULL;
    free(sl);
}

void lk_stringlist_append_lkstring(LKStringList *sl, lkstring.LKString *lks) {
    assert(sl->items_len <= sl->items_size);

    if (sl->items_len == sl->items_size) {
        lkstring.LKString **pitems = realloc(sl->items, (sl->items_size+N_GROW_STRINGLIST) * sizeof(lkstring.LKString));
        if (pitems == NULL) {
            return;
        }
        sl->items = pitems;
        sl->items_size += N_GROW_STRINGLIST;
    }
    sl->items[sl->items_len] = lks;
    sl->items_len++;

    assert(sl->items_len <= sl->items_size);
}
