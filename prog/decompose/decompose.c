// Strips log embelishments from docker-compose output.

int main() {
    char line[4096];


    while (!feof(stdin)) {
        fgets(line, sizeof(line), stdin);


        char *p = line;

        // {color}service_1                                     |ESC[0m info

        p = termcolor(p);

        // name
        while (!isspace(*p)) p++;

        // spaces
        while (isspace(*p)) p++;

        if (*p == '|') p++;

        p = termcolor(p);

        // while (isspace(*p)) p++;

        printf("%s", p);
    }


    return 0;
}

// ESC[0m
char *termcolor(char *p) {
    char *p0 = p;

    if (*p == 0x1B) p++; else return p0;
    if (*p == '[') p++; else return p0;

    // num
    while (isdigit(*p)) p++;

    // ;num
    if (*p == ';') {
        while (isdigit(*p)) p++;
    }
    if (*p == 'm') p++; else return p0;
    return p;
}
