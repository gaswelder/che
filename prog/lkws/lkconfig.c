#import fs
#import strings

#import lkhostconfig.c
#import lkstring.c
#import lkstringtable.c

pub typedef {
    char *serverhost;
    char *port;
    lkhostconfig.LKHostConfig **hostconfigs;
    size_t hostconfigs_len;
    size_t hostconfigs_size;
} LKConfig;

const int HOSTCONFIGS_INITIAL_SIZE = 10;

pub LKConfig *lk_config_new() {
    LKConfig *cfg = calloc(1, sizeof(LKConfig));
    cfg->serverhost = strings.newstr("%s", "localhost");
    cfg->port = strings.newstr("8000");
    cfg->hostconfigs = calloc(HOSTCONFIGS_INITIAL_SIZE, sizeof(lkhostconfig.LKHostConfig));
    cfg->hostconfigs_len = 0;
    cfg->hostconfigs_size = HOSTCONFIGS_INITIAL_SIZE;
    return cfg;
}

pub void lk_config_free(LKConfig *cfg) {
    free(cfg->serverhost);
    free(cfg->port);
    for (size_t i = 0; i < cfg->hostconfigs_len; i++) {
        lkhostconfig.LKHostConfig *hc = cfg->hostconfigs[i];
        lk_hostconfig_free(hc);
    }
    memset(cfg->hostconfigs, 0, sizeof(lkhostconfig.LKHostConfig) * cfg->hostconfigs_size);
    free(cfg->hostconfigs);
    cfg->hostconfigs = NULL;
    free(cfg);
}

pub lkhostconfig.LKHostConfig *lk_config_add_hostconfig(LKConfig *cfg, lkhostconfig.LKHostConfig *hc) {
    assert(cfg->hostconfigs_len <= cfg->hostconfigs_size);

    // Increase size if no more space.
    if (cfg->hostconfigs_len == cfg->hostconfigs_size) {
        cfg->hostconfigs_size++;
        cfg->hostconfigs = realloc(cfg->hostconfigs, sizeof(lkhostconfig.LKHostConfig) * cfg->hostconfigs_size);
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
        for (size_t i = 0; i < cfg->hostconfigs_len; i++) {
            lkhostconfig.LKHostConfig *hc = cfg->hostconfigs[i];
            if (lkstring.lk_string_sz_equal(hc->hostname, hostname)) {
                return hc;
            }
        }
    }
    // If hostname not found, return hostname * (fallthrough hostname).
    for (size_t i = 0; i < cfg->hostconfigs_len; i++) {
        lkhostconfig.LKHostConfig *hc = cfg->hostconfigs[i];
        if (lkstring.lk_string_sz_equal(hc->hostname, "*")) {
            return hc;
        }
    }
    return NULL;
}

// Return hostconfig with hostname or NULL if not found.
pub lkhostconfig.LKHostConfig *get_hostconfig(LKConfig *cfg, char *hostname) {
    for (size_t i = 0; i < cfg->hostconfigs_len; i++) {
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
    printf("serverhost: %s\n", cfg->serverhost);
    printf("port: %s\n", cfg->port);

    for (size_t i = 0; i < cfg->hostconfigs_len; i++) {
        lkhostconfig.LKHostConfig *hc = cfg->hostconfigs[i];
        printf("%2lu. hostname %s\n", i+1, hc->hostname->s);
        if (hc->homedir->s_len > 0) {
            printf("    homedir: %s\n", hc->homedir->s);
        }
        if (hc->cgidir->s_len > 0) {
            printf("    cgidir: %s\n", hc->cgidir->s);
        }
        if (hc->proxyhost->s_len > 0) {
            printf("    proxyhost: %s\n", hc->proxyhost->s);
        }
        for (size_t j = 0; j < hc->aliases->items_len; j++) {
            printf("    alias %s=%s\n", hc->aliases->items[j].k->s, hc->aliases->items[j].v->s);
        }
    }
    printf("\n");
}

// Fill in default values for unspecified settings.
pub void lk_config_finalize(LKConfig *cfg) {
    // Get current working directory.
    lkstring.LKString *current_dir = lkstring.lk_string_new("");
    char *s = get_current_dir_name();
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
    for (size_t i = 0; i < cfg->hostconfigs_len; i++) {
        lkhostconfig.LKHostConfig *hc = cfg->hostconfigs[i];

        // Skip hostconfigs that don't have have homedir.
        if (hc->homedir->s_len == 0) {
            continue;
        }

        // Set absolute path to homedir
        if (!fs.realpath(hc->homedir->s, homedir_abspath, sizeof(homedir_abspath))) {
            lk_print_err("realpath()");
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
    lkhostconfig.LKHostConfig *hc = calloc(1, sizeof(lkhostconfig.LKHostConfig));
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

    free(hc);
}

char *get_current_dir_name() {
    return "todo";
}

void lk_print_err(char *s) {
    fprintf(stderr, "%s: %s\n", s, strerror(errno));
}