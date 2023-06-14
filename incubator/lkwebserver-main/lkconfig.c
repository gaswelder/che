#import fs

#import lkhostconfig.c
#import lklib.c
#import lkstring.c
#import lkstringtable.c

pub typedef {
    lkstring.LKString *serverhost;
    lkstring.LKString *port;
    lkhostconfig.LKHostConfig **hostconfigs;
    size_t hostconfigs_len;
    size_t hostconfigs_size;
} LKConfig;

const int HOSTCONFIGS_INITIAL_SIZE = 10;

pub LKConfig *lk_config_new() {
    LKConfig *cfg = lkalloc.lk_malloc(sizeof(LKConfig), "lk_config_new");
    cfg->serverhost = lkstring.lk_string_new("");
    cfg->port = lkstring.lk_string_new("");
    cfg->hostconfigs = lkalloc.lk_malloc(sizeof(lkhostconfig.LKHostConfig) * HOSTCONFIGS_INITIAL_SIZE, "lk_config_new_hostconfigs");
    cfg->hostconfigs_len = 0;
    cfg->hostconfigs_size = HOSTCONFIGS_INITIAL_SIZE;
    return cfg;
}

pub void lk_config_free(LKConfig *cfg) {
    lkstring.lk_string_free(cfg->serverhost);
    lkstring.lk_string_free(cfg->port);
    for (int i=0; i < cfg->hostconfigs_len; i++) {
        lkhostconfig.LKHostConfig *hc = cfg->hostconfigs[i];
        lk_hostconfig_free(hc);
    }
    memset(cfg->hostconfigs, 0, sizeof(lkhostconfig.LKHostConfig) * cfg->hostconfigs_size);
    lkalloc.lk_free(cfg->hostconfigs);

    cfg->serverhost = NULL;
    cfg->port = NULL;
    cfg->hostconfigs = NULL;
    
    lkalloc.lk_free(cfg);
}


#define CONFIG_LINE_SIZE 255

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
enum {CFG_ROOT, CFG_HOSTSECTION};

pub int lk_config_read_configfile(LKConfig *cfg, char *configfile) {
    FILE *f = fopen(configfile, "r");
    if (f == NULL) {
        lklib.lklib.lk_print_err("lk_read_configfile fopen()");
        return -1;
    }

    char line[CONFIG_LINE_SIZE];
    int state = CFG_ROOT;
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
        lkstring.lk_string_split_assign(l, " ", k, v); // l:"k v", assign k and v
        if (lkstring.lk_string_sz_equal(k, "hostname")) {
            // hostname littlekitten.xyz
            hc = lk_config_create_get_hostconfig(cfg, v->s);
            state = CFG_HOSTSECTION;
            continue;
        }

        if (state == CFG_ROOT) {
            assert(hc == NULL);

            // serverhost=127.0.0.1
            // port=8000
            lkstring.lk_string_split_assign(l, "=", k, v); // l:"k=v", assign k and v
            if (lkstring.lk_string_sz_equal(k, "serverhost")) {
                lkstring.lk_string_assign(cfg->serverhost, v->s);
                continue;
            } else if (lkstring.lk_string_sz_equal(k, "port")) {
                lkstring.lk_string_assign(cfg->port, v->s);
                continue;
            }
            continue;
        }
        if (state == CFG_HOSTSECTION) {
            assert(hc != NULL);

            // homedir=testsite
            // cgidir=cgi-bin
            // proxyhost=localhost:8001
            lkstring.lk_string_split_assign(l, "=", k, v);
            if (lkstring.lk_string_sz_equal(k, "homedir")) {
                lkstring.lk_string_assign(hc->homedir, v->s);
                continue;
            } else if (lkstring.lk_string_sz_equal(k, "cgidir")) {
                lkstring.lk_string_assign(hc->cgidir, v->s);
                continue;
            } else if (lkstring.lk_string_sz_equal(k, "proxyhost")) {
                lkstring.lk_string_assign(hc->proxyhost, v->s);
                continue;
            }
            // alias latest=latest.html
            lkstring.lk_string_split_assign(l, " ", k, v);
            if (lkstring.lk_string_sz_equal(k, "alias")) {
                lkstring.lk_string_split_assign(v, "=", aliask, aliasv);
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
    return 0;
}

pub lkhostconfig.LKHostConfig *lk_config_add_hostconfig(LKConfig *cfg, lkhostconfig.LKHostConfig *hc) {
    assert(cfg->hostconfigs_len <= cfg->hostconfigs_size);

    // Increase size if no more space.
    if (cfg->hostconfigs_len == cfg->hostconfigs_size) {
        cfg->hostconfigs_size++;
        cfg->hostconfigs = lkalloc.lk_realloc(cfg->hostconfigs, sizeof(lkhostconfig.LKHostConfig) * cfg->hostconfigs_size, "lk_config_add_hostconfig");
    }
    cfg->hostconfigs[cfg->hostconfigs_len] = hc;
    cfg->hostconfigs_len++;

    return hc;
}

// Return hostconfig matching hostname,
// or if hostname parameter is NULL, return hostconfig matching "*".
// Return NULL if no matching hostconfig.
pub lkhostconfig.LKHostConfig *lk_config_find_hostconfig(LKConfig *cfg, char *hostname) {
    if (hostname != NULL) {
        for (int i=0; i < cfg->hostconfigs_len; i++) {
            lkhostconfig.LKHostConfig *hc = cfg->hostconfigs[i];
            if (lkstring.lk_string_sz_equal(hc->hostname, hostname)) {
                return hc;
            }
        }
    }
    // If hostname not found, return hostname * (fallthrough hostname).
    for (int i=0; i < cfg->hostconfigs_len; i++) {
        lkhostconfig.LKHostConfig *hc = cfg->hostconfigs[i];
        if (lkstring.lk_string_sz_equal(hc->hostname, "*")) {
            return hc;
        }
    }
    return NULL;
}

// Return hostconfig with hostname or NULL if not found.
pub lkhostconfig.LKHostConfig *get_hostconfig(LKConfig *cfg, char *hostname) {
    for (int i=0; i < cfg->hostconfigs_len; i++) {
        lkhostconfig.LKHostConfig *hc = cfg->hostconfigs[i];
        if (lkstring.lk_string_sz_equal(hc->hostname, hostname)) {
            return hc;
        }
    }
    return NULL;
}

// Return hostconfig with hostname if it exists.
// Create new hostconfig with hostname if not found. Never returns null.
pub lkhostconfig.LKHostConfig *lk_config_create_get_hostconfig(LKConfig *cfg, char *hostname) {
    lkhostconfig.LKHostConfig *hc = get_hostconfig(cfg, hostname);
    if (hc == NULL) {
        hc = lk_hostconfig_new(hostname);
        lk_config_add_hostconfig(cfg, hc);
    }
    return hc;
}

pub void lk_config_print(LKConfig *cfg) {
    printf("serverhost: %s\n", cfg->serverhost->s);
    printf("port: %s\n", cfg->port->s);

    for (int i=0; i < cfg->hostconfigs_len; i++) {
        lkhostconfig.LKHostConfig *hc = cfg->hostconfigs[i];
        printf("%2d. hostname %s\n", i+1, hc->hostname->s);
        if (hc->homedir->s_len > 0) {
            printf("    homedir: %s\n", hc->homedir->s);
        }
        if (hc->cgidir->s_len > 0) {
            printf("    cgidir: %s\n", hc->cgidir->s);
        }
        if (hc->proxyhost->s_len > 0) {
            printf("    proxyhost: %s\n", hc->proxyhost->s);
        }
        for (int j=0; j < hc->aliases->items_len; j++) {
            printf("    alias %s=%s\n", hc->aliases->items[j].k->s, hc->aliases->items[j].v->s);
        }
    }
    printf("\n");
}

// Fill in default values for unspecified settings.
pub void lk_config_finalize(LKConfig *cfg) {
    // serverhost defaults to localhost if not specified.
    if (cfg->serverhost->s_len == 0) {
        lkstring.lk_string_assign(cfg->serverhost, "localhost");
    }
    // port defaults to 8000 if not specified.
    if (cfg->port->s_len == 0) {
        lkstring.lk_string_assign(cfg->port, "8000");
    }

    // Get current working directory.
    lkstring.LKString *current_dir = lkstring.lk_string_new("");
    char *s = OS.get_current_dir_name();
    if (s != NULL) {
        lkstring.lk_string_assign(current_dir, s);
        free(s);
    } else {
        lkstring.lk_string_assign(current_dir, ".");
    }

    // Set fallthrough defaults only if no other hostconfigs specified
    // Note: If other hostconfigs are set, such as in a config file,
    // the fallthrough '*' hostconfig should be set explicitly. 
    if (cfg->hostconfigs_len == 0) {
        // homedir default to current directory
        lkhostconfig.LKHostConfig *hc = lk_config_create_get_hostconfig(cfg, "*");
        lkstring.lk_string_assign(hc->homedir, current_dir->s);

        // cgidir default to cgi-bin
        if (hc->cgidir->s_len == 0) {
            lkstring.lk_string_assign(hc->cgidir, "/cgi-bin/");
        }
    }

    // Set homedir absolute paths for hostconfigs.
    // Adjust /cgi-bin/ paths.
    char homedir_abspath[PATH_MAX];
    for (int i=0; i < cfg->hostconfigs_len; i++) {
        lkhostconfig.LKHostConfig *hc = cfg->hostconfigs[i];

        // Skip hostconfigs that don't have have homedir.
        if (hc->homedir->s_len == 0) {
            continue;
        }

        // Set absolute path to homedir
        if (!fs.realpath(hc->homedir->s, homedir_abspath, sizeof(homedir_abspath))) {
            lklib.lk_print_err("realpath()");
            homedir_abspath[0] = '\0';
        }
        lkstring.lk_string_assign(hc->homedir_abspath, homedir_abspath);

        // Adjust cgidir paths.
        if (hc->cgidir->s_len > 0) {
            if (!lkstring.lk_string_starts_with(hc->cgidir, "/")) {
                lkstring.lk_string_prepend(hc->cgidir, "/");
            }
            if (!lkstring.lk_string_ends_with(hc->cgidir, "/")) {
                lkstring.lk_string_append(hc->cgidir, "/");
            }

            lkstring.lk_string_assign(hc->cgidir_abspath, hc->homedir_abspath->s);
            lkstring.lk_string_append(hc->cgidir_abspath, hc->cgidir->s);
        }
    }

    lkstring.lk_string_free(current_dir);
}


pub lkhostconfig.LKHostConfig *lk_hostconfig_new(char *hostname) {
    lkhostconfig.LKHostConfig *hc = lkalloc.lk_malloc(sizeof(lkhostconfig.LKHostConfig), "lk_hostconfig_new");

    hc->hostname = lkstring.lk_string_new(hostname);
    hc->homedir = lkstring.lk_string_new("");
    hc->homedir_abspath = lkstring.lk_string_new("");
    hc->cgidir = lkstring.lk_string_new("");
    hc->cgidir_abspath = lkstring.lk_string_new("");
    hc->aliases = lkstringtable.lk_stringtable_new();
    hc->proxyhost = lkstring.lk_string_new("");

    return hc;
}

pub void lk_hostconfig_free(lkhostconfig.LKHostConfig *hc) {
    lkstring.lk_string_free(hc->hostname);
    lkstring.lk_string_free(hc->homedir);
    lkstring.lk_string_free(hc->homedir_abspath);
    lkstring.lk_string_free(hc->cgidir);
    lkstring.lk_string_free(hc->cgidir_abspath);
    lkstringtable.lk_stringtable_free(hc->aliases);
    lkstring.lk_string_free(hc->proxyhost);

    hc->hostname = NULL;
    hc->homedir = NULL;
    hc->homedir_abspath = NULL;
    hc->cgidir = NULL;
    hc->cgidir_abspath = NULL;
    hc->aliases = NULL;
    hc->proxyhost = NULL;

    lkalloc.lk_free(hc);
}
