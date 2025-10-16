
#define MAXCOLS 10

int main(int argc, char *argv[]) {
    // Parse the requested column numbers.
    int sel[10] = {};
    int nsel = 0;
    for (int i = 1; i < argc; i++) {
        int x;
        sscanf(argv[i], "%d", &x);
        sel[nsel++] = x-1; // adjust for 1-based counts
    }

    char line[4096];
    char *cols[MAXCOLS] = {};
    int ncols = 0;
    while (fgets(line, sizeof(line), stdin)) {
        ncols = 0;

        // Split the line
        char *p = line;
        char *q = sep(p);
        cols[ncols++] = p;
        while (*q != '\0') {
            p = q;
            q = sep(q);
            cols[ncols++] = p;
        }

        for (int i = 0; i < nsel; i++) {
            int index = sel[i];
            if (i > 0) putchar('\t');
            if (index >= 0 && index < ncols) {
                printf("%s", cols[index]);
            }
        }
        putchar('\n');
    }
    return 0;
}

char *sep(char *p) {
    while (*p != '\0' && !isspace(*p)) p++;
    if (*p == '\0') return p;
    *p++ = '\0';
    while (*p != '\0' && isspace(*p)) p++;
    return p;
}