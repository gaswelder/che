#import fs
#import strings

#import lkhostconfig.c
#import lkstring.c
#import lkstringtable.c

#define MAX_HOST_CONFIGS 256

pub typedef {
    char *serverhost;
    char *port;
    lkhostconfig.LKHostConfig *hostconfigs[256];
    size_t hostconfigs_size;
} LKConfig;

pub LKConfig *lk_config_new() {
    LKConfig *cfg = calloc(1, sizeof(LKConfig));
    cfg->serverhost = strings.newstr("%s", "localhost");
    cfg->port = strings.newstr("8000");
    return cfg;
}

pub void print(LKConfig *cfg) {
    printf("serverhost: %s\n", cfg->serverhost);
    printf("port: %s\n", cfg->port);

    for (size_t i = 0; i < cfg->hostconfigs_size; i++) {
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

void add_hostconfig(LKConfig *cfg, lkhostconfig.LKHostConfig *hc) {
    if (cfg->hostconfigs_size >= MAX_HOST_CONFIGS) {
        abort();
    }
    cfg->hostconfigs[cfg->hostconfigs_size++] = hc;
}

pub void lk_config_free(LKConfig *cfg) {
    for (size_t i = 0; i < cfg->hostconfigs_size; i++) {
        lkhostconfig.LKHostConfig *hc = cfg->hostconfigs[i];
        lk_hostconfig_free(hc);
    }
    free(cfg->serverhost);
    free(cfg->port);
    free(cfg);
}


// Return hostconfig matching hostname,
// or if hostname parameter is NULL, return hostconfig matching "*".
// Return NULL if no matching hostconfig.
pub lkhostconfig.LKHostConfig *lk_config_find_hostconfig(LKConfig *cfg, char *hostname) {
    if (hostname != NULL) {
        for (size_t i = 0; i < cfg->hostconfigs_size; i++) {
            lkhostconfig.LKHostConfig *hc = cfg->hostconfigs[i];
            if (lkstring.lk_string_sz_equal(hc->hostname, hostname)) {
                return hc;
            }
        }
    }
    // If hostname not found, return hostname * (fallthrough hostname).
    for (size_t i = 0; i < cfg->hostconfigs_size; i++) {
        lkhostconfig.LKHostConfig *hc = cfg->hostconfigs[i];
        if (lkstring.lk_string_sz_equal(hc->hostname, "*")) {
            return hc;
        }
    }
    return NULL;
}

// Return hostconfig with hostname or NULL if not found.
pub lkhostconfig.LKHostConfig *get_hostconfig(LKConfig *cfg, char *hostname) {
    for (size_t i = 0; i < cfg->hostconfigs_size; i++) {
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
        add_hostconfig(cfg, hc);
    }
    return hc;
}



lkhostconfig.LKHostConfig *default_hostconfig() {
    lkhostconfig.LKHostConfig *hc = lk_hostconfig_new("*");
    if (!hc) {
        return NULL;
    }
    lkstring.lk_string_assign(hc->homedir, get_current_dir_name());
    lkstring.lk_string_assign(hc->cgidir, "/cgi-bin/");
    return hc;
}

// Fill in default values for unspecified settings.
pub void lk_config_finalize(LKConfig *cfg) {
    // If no hostconfigs, add a fallthrough '*' hostconfig.
    if (cfg->hostconfigs_size == 0) {
        add_hostconfig(cfg, default_hostconfig());
    }

    // Set homedir absolute paths for hostconfigs.
    // Adjust /cgi-bin/ paths.
    char homedir_abspath[PATH_MAX];
    for (size_t i = 0; i < cfg->hostconfigs_size; i++) {
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
