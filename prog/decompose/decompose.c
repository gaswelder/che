// Strips log embelishments from docker-compose output.

#import linereader

int main() {
    OS.setvbuf(stdout, NULL, OS._IOLBF, 0);
	linereader.t *lr = linereader.new(stdin);
    while (linereader.read(lr)) {
        char *p = linereader.line(lr);

        // color, container name, spaces, "|", ESC[0m
		p = termcolor(p);

        char name[100] = {};
        int len = 0;
        while (!isspace(*p)) {
            name[len++] = *p++;
        }

        while (isspace(*p)) p++;

        if (*p == '|') p++;
        p = termcolor(p);
        while (isspace(*p)) p++;

        if (p[0] == '{') {
            printf("{\"container\":\"%s\",", name);
            printf("%s", &p[1]);
        } else {
            printf("[%s] %s", name, p);
        }
    }
	linereader.free(lr);
    return 0;
}

// ESC[0m
// ESC[36;1m
char *termcolor(char *p) {
    char *p0 = p;

    if (*p == 0x1B) p++; else return p0;
    if (*p == '[') p++; else return p0;

    // num
	// num ";" num
    while (isdigit(*p)) p++;
    if (*p == ';') {
        p++;
        while (isdigit(*p)) p++;
    }
	// m
    if (*p == 'm') p++; else return p0;
    return p;
}
