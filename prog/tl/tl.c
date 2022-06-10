#import strutil
#import opt
#import client.c

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s add | list | rm | hide | unhide\n", argv[0]);
        return 1;
    }
    char *cmd = argv[1];
    if (strstr("add", cmd)) {
        return cmd_add(argc - 1, argv + 1);
    }
    if (strstr("list", cmd)) {
        return cmd_list(argc - 1, argv + 1);
    }
    if (strstr("rm", cmd)) {
        return cmd_rm(argc - 1, argv + 1);
    }
    if (strstr("hide", cmd)) {
        return cmd_hide(argc - 1, argv + 1);
    }
    if (strstr("unhide", cmd)) {
        return cmd_unhide(argc - 1, argv + 1);
    }
    fprintf(stderr, "unknown command: %s\n", cmd);
    return 1;
}

int cmd_add(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <torrent-url> ...\n", argv[0]);
        return 1;
    }

    for (char **url = argv + 1; *url; url++) {
        tl_add(*url);
    }
    return 0;
}

int cmd_hide(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <torrent id> ...\n", argv[0]);
        return 1;
    }
    torr_t *tt = NULL;
    size_t len = 0;
    torrents(&tt, &len);
    FILE *f = fopen("tl.list", "a+");
    for (char **id = argv + 1; *id; id++) {
        for (size_t i = 0; i < len; i++) {
            torr_t l = tt[i];
            if (!strcmp(*id, l.id)) {
                fprintf(f, "%s\n", l.name);
                break;
            }
        }
    }
    free(tt);
    fclose(f);
    return 0;
}

int cmd_list(int argc, char **argv) {
    bool all = false;
    opt(OPT_BOOL, "a", "show all torrents", &all);
    bool hidden = false;
    opt(OPT_BOOL, "i", "show hidden torrents", &hidden);
    bool order = false;
    opt(OPT_BOOL, "r", "order by seed ratio", &order);
    opt_parse(argc, argv);

    bool showhidden = all || hidden;
    bool showvisible = all || !hidden;

    torr_t *tt = NULL;
    size_t len = 0;
    torrents(&tt, &len);
    if (order) {
        qsort(tt, len, sizeof(torr_t), &cmp);
    }

    FILE *f = fopen("tl.list", "r");

    for (size_t i = 0; i < len; i++) {
        torr_t l = tt[i];

        if (!showhidden && ishidden(f, l.name)) {
            continue;
        }
        if (!showvisible && !ishidden(f, l.name)) {
            continue;
        }

        printf("%s", l.id);
        if (strcmp(l.eta, "Done")) {
            printf(" (%s)", l.done);
        }
        printf("\t%s/%s, r=%s\t%s\n", l.up, l.down, l.ratio, l.name);
    }

    free(tt);
    if (f) fclose(f);
    return 0;
}

bool ishidden(FILE *f, char *name) {
    if (!f) return false;
    rewind(f);
    bool r = false;
    char buf[1000] = {};
    while (fgets(buf, sizeof(buf), f)) {
        if (!strcmp(trim(buf), name)) {
            r = true;
            break;
        }
    }
    return r;
}

int cmp(const void *va, *vb) {
    torr_t *a = (torr_t *) va;
    torr_t *b = (torr_t *) vb;
    float ar = 0;
    float br = 0;
    sscanf(a->ratio, "%f", &ar);
    sscanf(b->ratio, "%f", &br);
    if (ar > br) return -1;
    if (br > ar) return 1;
    return 0;
}

int cmd_rm(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <torrent-id> ...\n", argv[0]);
        return 1;
    }

    for (char **id = argv + 1; *id; id++) {
        char dir[1000] = {};
        if (!tl_getval(*id, "Location", dir, sizeof(dir))) {
            fprintf(stderr, "failed to get location for torrent %s\n", *id);
            continue;
        }
        char name[1000] = {};
        if (!tl_getval(*id, "Name", name, sizeof(name))) {
            fprintf(stderr, "failed to get name for torrent %s\n", *id);
            continue;
        }

        tl_rm(*id);

        char *path = newstr("%s/%s", dir, name);
        char *newpath = newstr("%s/__/%s", dir, name);
        if (rename(path, newpath) < 0) {
            fprintf(stderr, "failed to move torrent to '%s': %s\n", newpath, strerror(errno));
        }
        free(path);
        free(newpath);
    }

    return 0;
}

int cmd_unhide(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <torrent id> ...\n", argv[0]);
        return 1;
    }

    FILE *f = fopen("tl.list", "r");
    if (!f) {
        fprintf(stderr, "nothing to unhide (tl.list doesn't exist)\n");
        return 1;
    }
    FILE *tmp = fopen("tl.list.tmp", "w");
    if (!tmp) {
        fprintf(stderr, "failed to open tl.list.tmp: %s\n", strerror(errno));
        return 1;
    }

    torr_t *tt = NULL;
    size_t len = 0;
    torrents(&tt, &len);

    show(tt, len, f, tmp, argv + 1);

    fclose(tmp);
    fclose(f);
    free(tt);

    if (rename("tl.list.tmp", "tl.list") < 0) {
        fprintf(stderr, "failed to replace tl.list: %s\n", strerror(errno));
        return 1;
    }
    return 0;
}

void show(torr_t *tt, size_t len, FILE *f, *tmp, char **ids) {
    char buf[1000] = {};
    while (fgets(buf, sizeof(buf), f)) {
        char *name = trim(buf);
        /*
         * Find the torrent with this name
         */
        torr_t *t = NULL;
        for (size_t i = 0; i < len; i++) {
            if (!strcmp(tt[i].name, name)) {
                t = tt + i;
                break;
            }
        }

        /*
         * If torrent not found, GC
         */
        if (t == NULL) continue;

        /*
         * If id matches, omit this line.
         * Otherwise keep it.
         */
        if (contains(ids, t->id)) continue;
        fprintf(tmp, "%s\n", name);
    }
}

bool contains(char **ids, char *id) {
    for (char **p = ids; *p; p++) {
        if (!strcmp(*p, id)) {
            return true;
        }
    }
    return false;
}
