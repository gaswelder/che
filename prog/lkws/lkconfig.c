#import fs
#import strings
#import os/misc

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
        printf("%2lu. hostname %s\n", i+1, hc->hostname);
        if (strlen(hc->homedir) > 0) {
            printf("    homedir: %s\n", hc->homedir);
        }
        if (strlen(hc->cgidir) > 0) {
            printf("    cgidir: %s\n", hc->cgidir);
        }
        if (strlen(hc->proxyhost) > 0) {
            printf("    proxyhost: %s\n", hc->proxyhost);
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



lkhostconfig.LKHostConfig *default_hostconfig() {
    lkhostconfig.LKHostConfig *hc = lk_hostconfig_new("*");
    if (!hc) {
        return NULL;
    }
    if (!misc.getcwd(hc->homedir, sizeof(hc->homedir))) {
        panic("failed to get current working directory: %s", strerror(errno));
    }
    strcpy(hc->cgidir, "/cgi-bin/");
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
        if (strlen(hc->homedir) == 0) {
            continue;
        }

        // Set absolute path to homedir
        if (!fs.realpath(hc->homedir, homedir_abspath, sizeof(homedir_abspath))) {
            lk_print_err("realpath()");
            homedir_abspath[0] = '\0';
        }
        strcpy(hc->homedir_abspath, homedir_abspath);

        // Adjust cgidir paths.
        if (strlen(hc->cgidir) > 0) {
            char *tmp = strings.newstr("%s/%s/", hc->homedir_abspath, hc->cgidir);
            strcpy(hc->cgidir_abspath, tmp);
            free(tmp);
        }
    }
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

void lk_print_err(char *s) {
    fprintf(stderr, "%s: %s\n", s, strerror(errno));
}
