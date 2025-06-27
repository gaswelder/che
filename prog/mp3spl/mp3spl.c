#import formats/mp3
#import strings

/*
 * mp3spl <mp3> <time>,<time>,...
 *
 * Splits given MP3 file at given split points.
 * For N points produces N+1 files.
 */

int main(int argc, char *argv[]) {
	if (argc != 3) {
		fprintf(stderr, "Usage: mp3spl <mp3file> <m:s.sss...>,<m:s.sss...>,...\n");
		return 1;
	}

	const char *path = argv[1];
	mp3.mp3file *f = mp3.mp3open(path);
	if (mp3.mp3err(f)) {
		fprintf(stderr, "%s\n", mp3.mp3err(f));
		return 1;
	}

	mp3.mp3time_t points[20] = {};
	if (!parse_times(points, argv[2], 20)) {
		fprintf(stderr, "Couldn't parse time positions\n");
		return 1;
	}

	size_t len = strlen(path);
	/*
	 * The original name without the extension.
	 */
	char *namebase = calloc(len + 1, sizeof(char));
	strcpy(namebase, path);
	if(len > 4 && strcmp(namebase + len - 4, ".mp3") == 0) {
		namebase[len - 4] = '\0';
	}

	/*
	 * Buffer for new names.
	 * +3 to account for "-NN",
	 * +4 for ".mp3" in case the original name doesn't have it.
	 */
	size_t namesize = len + 1 + 3 + 4;
	char *newname = calloc(namesize, sizeof(char));

	for(int i = 0; i < 20; i++) {
		snprintf(newname, namesize, "%s-%02d.mp3", namebase, i+1);
		puts(newname);
		out(f, newname, points[i]);
		if(points[i].min == -1) {
			break;
		}
	}

	free(namebase);
	free(newname);
	mp3.mp3close(f);
	return 0;
}

bool parse_time(const char **spec, mp3.mp3time_t *t) {
	int min = 0;
	int sec = 0;
	int usec = 0;
	const char *p = *spec;

	// min
	if (!readint(&p, &min)) {
		return false;
	}
	// :
	if (*p++ != ':') {
		fprintf(stderr, "':' expected");
		return false;
	}
	// sec
	if (!readint(&p, &sec)) {
		return false;
	}
	// .usec
	if (*p == '.') {
		p++;
		if (!isdigit(*p)) {
			fprintf(stderr, "Digits expected after point: %s", p);
			return false;
		}
		int pow = 100000;
		while (isdigit(*p)) {
			usec += strings.num_from_ascii(*p) * pow;
			pow /= 10;
			p++;
		}
	}
	if(min >= 60 || sec >= 60) {
		fprintf(stderr, "Incorrect time: %d:%d", min, sec);
		return false;
	}
	*spec = p;
	t->min = min;
	t->sec = sec;
	t->usec = usec;
	return true;
}

bool readint(const char **spec, int *r) {
	const char *p = *spec;
	if (!isdigit(*p)) {
		fprintf(stderr, "Unexpected character: %c in %s\n", *p, p);
		return false;
	}
	int val = 0;
	while (isdigit(*p)) {
		val *= 10;
		val += strings.num_from_ascii(*p);
		p++;
	}
	*r = val;
	*spec = p;
	return true;
}

// <m:s.sss...>
bool parse_times(mp3.mp3time_t *a, const char *spec, size_t maxsize) {
	if (maxsize == 0) {
		panic("zero maxsize");
	}

	size_t i = 0;
	while (*spec != '\0') {
		if(i + 1 >= maxsize) {
			fprintf(stderr, "maxsize reached");
			return false;
		}
		if (!parse_time(&spec, &a[i])) {
			return false;
		}
		if (*spec == ',') {
			spec++;
		}
		i++;
	}

	a[i].min = -1;
	a[i].sec = -1;
	return true;
}

void out(mp3.mp3file *f, const char *path, mp3.mp3time_t t)
{
	if (mp3.mp3err(f)) {
		panic("%s", mp3.mp3err(f));
	}
	FILE *out = fopen(path, "wb");
	if (!out) {
		panic("fopen(%s) failed", path);
	}
	mp3.mp3out(f, out, t);
	fclose(out);
}
