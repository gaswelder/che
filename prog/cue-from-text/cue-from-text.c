#import strings
#import time

int main(int argc, char *argv[]) {
	if (argc != 3) {
		fprintf(stderr, "arguments: <band> <file>");
		return 1;
	}
	const char *band = argv[1];
    const char *file = argv[2];

    printf("FILE \"%s\" MP3\n", file);

	time.duration_t pos = {};
	int i = 0;

	char line[4096] = {};
	while (fgets(line, sizeof(line), stdin)) {
		track_t t = {};

		strings.trim(line);
		parse_line(line, &t);
		i++;
        emit(i, t.title, band, &pos);
		time.dur_add(&pos, &t.dur);
	}

    return 0;
}

void emit(int num, const char *title, *band, time.duration_t *pos) {
	char buf[10] = {};
    printf("TRACK %02d AUDIO\n", num);
    printf("  TITLE \"%s\"\n", title);
    printf("  PERFORMER \"%s\"\n", band);
    printf("  INDEX 01 ");
	if (!time.dur_fmt(pos, buf, sizeof(buf), "mm:ss")) {
		panic("failed to format track position");
	}
    printf("%s:00\n", buf);
}


typedef {
    char num[10];
    char title[100];
    time.duration_t dur;
} track_t;

void parse_line(const char *s, track_t *t) {
    const char *p = s;

    // 1
    char *tmp = t->num;
    while (isdigit(*p)) {
        *tmp++ = *p++;
    }

    // .
    if (*p != '.') {
        panic("dot expected after track number");
    }
    p++;

    // spaces
    while (isspace(*p)) {
        p++;
    }

    // title - everything except the time (5 chars)
    tmp = t->title;
    size_t title_len = strlen(s) - (size_t)(p-s) - 5;
    for (size_t i = 0; i < title_len; i++) {
        *tmp++ = *p++;
    }
	strings.trim(t->title);

	// 03:10
	time.parse_duration(p, &t->dur);
}
