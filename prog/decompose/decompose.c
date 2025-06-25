// Strips log embelishments from docker-compose output.

int main() {
    char line[4096];
    while (!feof(stdin)) {
        fgets(line, sizeof(line), stdin);
        char *p = line;

        // color, container name, spaces, "|", ESC[0m
		p = termcolor(p);
        while (!isspace(*p)) p++;
        while (isspace(*p)) p++;
        if (*p == '|') p++;
        p = termcolor(p);

		// message
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
	// num ";" num
    while (isdigit(*p)) p++;
    if (*p == ';') {
        while (isdigit(*p)) p++;
    }
	// m
    if (*p == 'm') p++; else return p0;
    return p;
}
