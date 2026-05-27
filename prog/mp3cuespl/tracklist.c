#import error
#import strings
#import time

/*
Parses tracklist lines like these:

01. Earth, Sky, Fire, Blood	4:22
02. Maestoso				5:24
03. Kusiciel				4:25

*/

pub typedef {
	int num;
	char title[1000];
	int64_t duration_us;
} entry_t;

pub entry_t *readfile(const char *path, int *ret_n) {
	FILE *f = fopen(path, "rb");
	if (!f) {
		panic("failed to open %s", path);
	}
	entry_t *list = calloc!(100, sizeof(entry_t));
	int i = 0;
	char line[4096];
	while (fgets(line, sizeof(line), f)) {
		entry_t *e = &list[i++];
		parseline(line, e);
	}
	fclose(f);
	*ret_n = i;
	return list;
}

void parseline(char *line, entry_t *e) {
	strings.trim(line);

	//
	// Get the duration
	//
	size_t i = strlen(line);
	while (i > 0 && !isspace(line[i-1])) {
		i--;
	}
	time.duration_t d = {};
	error.t err = {};
	if (!time.parse_duration(&line[i], &d, &err)) {
		panic("failed to parse duration: %s", err.msg);
	}
	line[i-1] = '\0';
	strings.trim(line);

	//
	// Get the number
	//
	i = 0;
	int num = 0;
	while (isdigit(line[i])) {
		num *= 10;
		num += strings.num_from_ascii(line[i]);
		i++;
	}
	if (line[i] != '.') {
		panic("expected a dot after track number");
	}

	//
	// Title
	//
	i++;
	while (isspace(line[i])) {
		i++;
	}

	//
	// Copy to the entry
	//
	e->num = num;
	strcpy(e->title, &line[i]);
	e->duration_us = time.dur_us(&d);
}
