#import formats/mp3

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

bool parse_times(mp3.mp3time_t *a, const char *spec, size_t maxsize)
{
	if (maxsize == 0) {
		panic("zero maxsize");
	}

	size_t i = 0;
	while(*spec) {
		if(i + 1 >= maxsize) {
			fprintf(stderr, "maxsize reached");
			return false;
		}

		int min = 0;
		int sec = 0;
		int usec = 0;

		if(!isdigit(*spec)) {
			fprintf(stderr, "Unexpected char: %c in %s\n", *spec, spec);
			return false;
		}

		// min
		while(isdigit(*spec)) {
			min *= 10;
			min += (*spec - '0');
			spec++;
		}

		// :
		if(*spec != ':') {
			fprintf(stderr, "':' expected");
			return false;
		}
		spec++;

		// sec
		if(!isdigit(*spec)) {
			fprintf(stderr, "Unexpected char: %c in %s", *spec, spec);
			return false;
		}
		while(isdigit(*spec)) {
			sec *= 10;
			sec += (*spec - '0');
			spec++;
		}
		if(*spec == '.') {
			spec++;
			if(!isdigit(*spec)) {
				fprintf(stderr, "Digits expected after point: %s", spec);
				return false;
			}
			int pow = 100000;
			while(isdigit(*spec)) {
				usec += (*spec - '0') * pow;
				pow /= 10;
				spec++;
			}
		}

		if(*spec == ',') {
			spec++;
		}

		if(min >= 60 || sec >= 60) {
			fprintf(stderr, "Incorrect time: %d:%d", min, sec);
			return false;
		}

		a[i].min = min;
		a[i].sec = sec;
		a[i].usec = usec;

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
