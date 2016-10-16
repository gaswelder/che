import "cli"
import "mp3"

/*
 * mp3spl <mp3> <time>,<time>,...
 *
 * Splits given MP3 file at given split points.
 * For N points produces N+1 files.
 */

int main(int argc, char *argv[])
{
	if(argc != 3) {
		fatal("Usage: mp3spl <mp3file> <m:s.sss...>,<m:s.sss...>,...");
	}

	const char *path = argv[1];
	mp3file *f = mp3open(path);
	if(mp3err(f)) {
		fatal("%s", mp3err(f));
	}

	struct mp3time points[20];
	if(!parse_times(points, argv[2], 20)) {
		fatal("Couldn't parse time positions");
	}

	size_t len = strlen(path);
	/*
	 * The original name without the extension.
	 */
	char namebase[len + 1];
	strcpy(namebase, path);
	if(len > 4 && strcmp(namebase + len - 4, ".mp3") == 0) {
		namebase[len - 4] = '\0';
	}

	/*
	 * Buffer for new names.
	 * +3 to account for "-NN",
	 * +4 for ".mp3" in case the original name doesn't have it.
	 */
	char newname[len + 1 + 3 + 4];

	for(int i = 0; i < 20; i++) {
		snprintf(newname, sizeof(newname), "%s-%02d.mp3", namebase, i+1);
		puts(newname);
		out(f, newname, points[i]);
		if(points[i].min == -1) {
			break;
		}
	}

	mp3close(f);
}

bool parse_times(struct mp3time *a, const char *spec, size_t maxsize)
{
	size_t i = 0;

	if(maxsize == 0) {
		fatal("zero maxsize");
	}

	while(*spec) {
		if(i + 1 >= maxsize) {
			err("maxsize reached");
			return false;
		}

		int min = 0;
		int sec = 0;
		int usec = 0;

		if(!isdigit(*spec)) {
			err("Unexpected char: %c in %s\n", *spec, spec);
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
			err("':' expected");
			return false;
		}
		spec++;

		// sec
		if(!isdigit(*spec)) {
			err("Unexpected char: %c in %s", *spec, spec);
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
				err("Digits expected after point: %s", spec);
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
			err("Incorrect time: %d:%d", min, sec);
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

void out(mp3file *f, const char *path, struct mp3time t)
{
	if(mp3err(f)) {
		fatal("%s", mp3err(f));
	}

	FILE *out = fopen(path, "wb");
	if(!out) {
		fatal("fopen(%s) failed", path);
	}

	mp3out(f, out, t);
	fclose(out);
}
