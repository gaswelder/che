
#define MAXCOLS 100
#define MAXSEL 10

int main(int argc, char *argv[]) {
    //
    // Parse the requested column numbers.
    //
    int sel[MAXSEL] = {};
    int nsel = 0;
    for (int i = 1; i < argc; i++) {
        if (nsel == MAXSEL) {
            fprintf(stderr, "too many requested cols, max is %d\n", MAXSEL);
            return 1;
        }
        int x;
        sscanf(argv[i], "%d", &x);
        sel[nsel++] = x-1; // adjust for 1-based counts
    }

    char line[4096];
    while (fgets(line, sizeof(line), stdin)) {
        //
        // Split the line into columns in place.
        //
        char *cols[MAXCOLS] = {};
        int ncols = 0;
        char *p = line;
        cols[ncols++] = p;
        while (true) {
            p = sep(p);
            if (*p == '\0') {
                break;
            }
            if (ncols == MAXCOLS) {
                fprintf(stderr, "input has too many columns, max supported is %d\n", MAXCOLS);
                return 1;
            }
            cols[ncols++] = p;
        }
        //
        // Print the requested values.
        //
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