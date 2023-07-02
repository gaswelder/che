#import clip/stringtable

pub typedef {
    char hostname[1000];
    char homedir[1000];
    char homedir_abspath[1000];
    char cgidir[1000];
    char cgidir_abspath[1000];
    char proxyhost[1000];

    stringtable.t *aliases;
} LKHostConfig;

pub typedef {
    LKHostConfig *hostconfigs[256];
    size_t hostconfigs_size;
} LKConfig;

pub LKConfig *lk_config_new() {
    LKConfig *cfg = calloc(1, sizeof(LKConfig));
    return cfg;
}

pub void print(LKConfig *cfg) {
    for (size_t i = 0; i < cfg->hostconfigs_size; i++) {
        LKHostConfig *hc = cfg->hostconfigs[i];
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
        for (size_t j = 0; j < hc->aliases->len; j++) {
            printf("    alias %s=%s\n", hc->aliases->items[j].k, hc->aliases->items[j].v);
        }
        printf("------\n");
    }
    printf("\n");
}

pub void add_hostconfig(LKConfig *cfg, LKHostConfig *hc) {
    cfg->hostconfigs[cfg->hostconfigs_size++] = hc;
}

// Return hostconfig matching hostname,
// or if hostname parameter is NULL, return hostconfig matching "*".
// Return NULL if no matching host
pub LKHostConfig *lk_config_find_hostconfig(LKConfig *cfg, char *hostname) {
    if (hostname != NULL) {
        for (size_t i = 0; i < cfg->hostconfigs_size; i++) {
            LKHostConfig *hc = cfg->hostconfigs[i];
            if (!strcmp(hc->hostname, hostname)) {
                return hc;
            }
        }
    }
    // If hostname not found, return hostname * (fallthrough hostname).
    for (size_t i = 0; i < cfg->hostconfigs_size; i++) {
        LKHostConfig *hc = cfg->hostconfigs[i];
        if (!strcmp(hc->hostname, "*")) {
            return hc;
        }
    }
    return NULL;
}

// Return hostconfig with hostname or NULL if not found.
LKHostConfig *get_hostconfig(LKConfig *cfg, char *hostname) {
    for (size_t i = 0; i < cfg->hostconfigs_size; i++) {
        LKHostConfig *hc = cfg->hostconfigs[i];
        if (!strcmp(hc->hostname, hostname)) {
            return hc;
        }
    }
    return NULL;
}

// Return hostconfig with hostname if it exists.
// Create new hostconfig with hostname if not found. Never returns null.
pub LKHostConfig *lk_config_create_get_hostconfig(LKConfig *cfg, char *hostname) {
    LKHostConfig *hc = get_hostconfig(cfg, hostname);
    if (hc == NULL) {
        hc = lk_hostconfig_new(hostname);
        add_hostconfig(cfg, hc);
    }
    return hc;
}

pub LKHostConfig *lk_hostconfig_new(char *hostname) {
    LKHostConfig *hc = calloc(1, sizeof(LKHostConfig));
    hc->aliases = stringtable.new();
    strcpy(hc->hostname, hostname);
    return hc;
}


