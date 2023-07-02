#import strings

#import lkhostconfig.c
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
        printf("--- vhost %zu ---\n", i);
        printf("hostname = %s\n", hc->hostname);
        if (strlen(hc->homedir) > 0) {
            printf("homedir = %s\n", hc->homedir);
        }
        if (strlen(hc->cgidir) > 0) {
            printf("cgidir = %s (%s)\n", hc->cgidir, hc->cgidir_abspath);
        }
        if (strlen(hc->proxyhost) > 0) {
            printf("proxyhost = %s\n", hc->proxyhost);
        }
        for (size_t j = 0; j < hc->aliases->items_len; j++) {
            printf("    alias %s=%s\n", hc->aliases->items[j].k->s, hc->aliases->items[j].v->s);
        }
        printf("------\n");
    }
    printf("\n");
}

pub void add_hostconfig(LKConfig *cfg, lkhostconfig.LKHostConfig *hc) {
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
            if (!strcmp(hc->hostname, hostname)) {
                return hc;
            }
        }
    }
    // If hostname not found, return hostname * (fallthrough hostname).
    for (size_t i = 0; i < cfg->hostconfigs_size; i++) {
        lkhostconfig.LKHostConfig *hc = cfg->hostconfigs[i];
        if (!strcmp(hc->hostname, "*")) {
            return hc;
        }
    }
    return NULL;
}

// Return hostconfig with hostname or NULL if not found.
pub lkhostconfig.LKHostConfig *get_hostconfig(LKConfig *cfg, char *hostname) {
    for (size_t i = 0; i < cfg->hostconfigs_size; i++) {
        lkhostconfig.LKHostConfig *hc = cfg->hostconfigs[i];
        if (!strcmp(hc->hostname, hostname)) {
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

pub lkhostconfig.LKHostConfig *lk_hostconfig_new(char *hostname) {
    lkhostconfig.LKHostConfig *hc = calloc(1, sizeof(lkhostconfig.LKHostConfig));
    hc->aliases = lkstringtable.lk_stringtable_new();
    strcpy(hc->hostname, hostname);
    return hc;
}

pub void lk_hostconfig_free(lkhostconfig.LKHostConfig *hc) {
    lkstringtable.lk_stringtable_free(hc->aliases);
    free(hc);
}
