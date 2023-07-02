#import clip/stringtable
#import strings

#import config.c

#define CONFIG_LINE_SIZE 255

enum {
    NONE,
    HOSTNAME,
    HOMEDIR,
    ALIAS,
    CGIDIR,
    PROXYHOST
};

int opcode(const char *op) {
    if (!strcmp(op, "hostname")) return HOSTNAME;
    if (!strcmp(op, "homedir")) return HOMEDIR;
    if (!strcmp(op, "alias")) return ALIAS;
    if (!strcmp(op, "cgidir")) return CGIDIR;
    if (!strcmp(op, "proxyhost")) return PROXYHOST;
    return NONE;
}

typedef {
    FILE *f;
    char buf[1000];
    char *p;
    char tmp[1000];
} parser_t;

bool readline(parser_t *parser) {
    while (true) {
        if (!fgets(parser->buf, 1000, parser->f)) {
            return false;
        }
        char *p = parser->buf;
        while (*p && isspace(*p)) {
            p++;
        }
        if (*p && *p != '#') {
            parser->p = p;
            return true;
        }
    }
}

char *readop(parser_t *parser) {
    if (!readline(parser)) {
        return NULL;
    }
    char *p = parser->p;
    char *q = parser->tmp;
    while (*p && isalpha(*p)) {
        *q++ = *p++;
    }
    parser->p = p;
    *q = '\0';
    return parser->tmp;
}

char *readspaceval(parser_t *parser) {
    char *p = parser->p;
    while (*p && isspace(*p)) {
        p++;
    }
    char *q = parser->tmp;
    while (*p && *p != '\r' && *p != '\n') {
        *q++ = *p++;
    }
    *q = '\0';
    parser->p = p;
    return parser->tmp;
}

char *readeqval(parser_t *parser) {
    char *p = parser->p;
    while (*p && isspace(*p)) {
        p++;
    }
    if (*p != '=') {
        return NULL;
    }
    p++;
    while (*p && isspace(*p)) {
        p++;
    }
    char *q = parser->tmp;
    while (*p && *p != '\r' && *p != '\n') {
        *q++ = *p++;
    }
    *q = '\0';
    parser->p = p;
    return parser->tmp;
}

pub config.LKConfig *read_file(const char *configfile) {
    FILE *f = fopen(configfile, "r");
    if (f == NULL) {
        return NULL;
    }
    parser_t parser = { .f = f };
    config.LKHostConfig *configs[100] = {NULL};
    size_t nconfigs = 0;

    while (1) {
        char *op = readop(&parser);
        if (!op) {
            break;
        }
        switch (opcode(op)) {
            case HOSTNAME:
                char *val = readspaceval(&parser);
                configs[nconfigs] = config.lk_hostconfig_new(val);
                nconfigs++;
                continue;

            case HOMEDIR:
                char *val = readeqval(&parser);
                strcpy(configs[nconfigs-1]->homedir, val);
                continue;

            case CGIDIR:
                char *val = readeqval(&parser);
                strcpy(configs[nconfigs-1]->cgidir, val);
                continue;

            case PROXYHOST:
                char *val = readeqval(&parser);
                strcpy(configs[nconfigs-1]->proxyhost, val);
                continue;

            case ALIAS:
                char *val = readspaceval(&parser);
                char *parts[2] = {0};
                strings.split("=", val, parts, 2);
                stringtable.set(configs[nconfigs-1]->aliases, parts[0], parts[1]);
                free(parts[0]);
                free(parts[1]);
                // todo prepend '/' if missing
                continue;
            default:
                panic("unknown op: %s", op);
        }
    }
    config.LKConfig *cfg = config.lk_config_new();
    for (size_t i = 0; i < nconfigs; i++) {
        config.add_hostconfig(cfg, configs[i]);
    }
    fclose(f);
    return cfg;
}
