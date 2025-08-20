#import client.c
#import opt
#import strings

int main(int argc, char **argv) {
	opt.addcmd("add", cmd_add);
	opt.addcmd("list", cmd_list);
	opt.addcmd("rm", cmd_rm);
	opt.addcmd("hide", cmd_hide);
	opt.addcmd("unhide", cmd_unhide);
	return opt.dispatch(argc, argv);
}

int cmd_add(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <torrent-url> ...\n", argv[0]);
        return 1;
    }

    for (char **url = argv + 1; *url; url++) {
        client.tl_add(*url);
    }
    return 0;
}

int cmd_hide(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <torrent id> ...\n", argv[0]);
        return 1;
    }
    client.torr_t *tt = NULL;
    size_t len = 0;
    client.torrents(&tt, &len);
    FILE *f = fopen("tl.list", "a+");
    for (char **id = argv + 1; *id; id++) {
        for (size_t i = 0; i < len; i++) {
            client.torr_t l = tt[i];
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
    opt.flag("a", "show all torrents", &all);
    bool hidden = false;
    opt.flag("i", "show hidden torrents", &hidden);
    bool order = false;
    opt.flag("r", "order by seed ratio", &order);
    opt.parse(argc, argv);

    bool showhidden = all || hidden;
    bool showvisible = all || !hidden;

    client.torr_t *tt = NULL;
    size_t len = 0;
    client.torrents(&tt, &len);
    if (order) {
        qsort(tt, len, sizeof(client.torr_t), &cmp);
    }

    FILE *f = fopen("tl.list", "r");

    for (size_t i = 0; i < len; i++) {
        client.torr_t l = tt[i];

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
		strings.trim(buf);
        if (!strcmp(buf, name)) {
            r = true;
            break;
        }
    }
    return r;
}

int cmp(const void *va, *vb) {
    const client.torr_t *a = va;
    const client.torr_t *b = vb;
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
        if (!client.tl_getval(*id, "Location", dir, sizeof(dir))) {
            fprintf(stderr, "failed to get location for torrent %s\n", *id);
            continue;
        }
        char name[1000] = {};
        if (!client.tl_getval(*id, "Name", name, sizeof(name))) {
            fprintf(stderr, "failed to get name for torrent %s\n", *id);
            continue;
        }

        client.tl_rm(*id);

        char *path = strings.newstr("%s/%s", dir, name);
        char *newpath = strings.newstr("%s/__%s", dir, name);
        if (rename(path, newpath) < 0) {
            fprintf(stderr, "failed rename torrent to '%s': %s\n", newpath, strerror(errno));
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

    client.torr_t *tt = NULL;
    size_t len = 0;
    client.torrents(&tt, &len);

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

void show(client.torr_t *tt, size_t len, FILE *f, *tmp, char **ids) {
    char buf[1000] = {};
    while (fgets(buf, sizeof(buf), f)) {
        strings.trim(buf);
		char *name = buf;

        /*
         * Find the torrent with this name
         */
        client.torr_t *t = NULL;
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
