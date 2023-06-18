#import strings
#import fs

const char *default_files[] = {
    "index.html",
    "index.htm",
    "default.html",
    "default.htm"
};

pub char *resolve(const char *homedir, *reqpath) {
    if (!strcmp(reqpath, "")) {
        for (size_t i = 0; i < nelem(default_files); i++) {
            char *p = resolve_inner(homedir, default_files[i]);
            if (p) {
                return p;
            }
        }
        return NULL;
    }
    return resolve_inner(homedir, reqpath);
}

char *resolve_inner(const char *homedir, *reqpath) {
    char naive_path[4096] = {0};
    sprintf(naive_path, "%s/%s", homedir, reqpath);
    printf("naive path = %s\n", naive_path);

    char realpath[4096] = {0};
    if (!fs.realpath(naive_path, realpath, sizeof(realpath))) {
        printf("couldn't resolve realpath\n");
        return NULL;
    }
    printf("real path = %s\n", realpath);

    if (!strings.starts_with(realpath, homedir)) {
        printf("resolved path doesn't start with homedir=%s\n", homedir);
        return NULL;
    }

    return strings.newstr("%s", realpath);
}
