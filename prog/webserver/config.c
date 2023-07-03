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

pub LKHostConfig *lk_hostconfig_new(char *hostname) {
    LKHostConfig *hc = calloc(1, sizeof(LKHostConfig));
    hc->aliases = stringtable.new();
    strcpy(hc->hostname, hostname);
    return hc;
}

pub void print(LKHostConfig *hc) {
    printf("--- vhost ---\n");
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
