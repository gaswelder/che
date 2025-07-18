#import formats/vtt

int main() {
	vtt.reader_t *r = vtt.reader(stdin);
	char buffer[10000] = {};
	while (vtt.read(r)) {
		strcpy(buffer, r->text);
		output(buffer);
	}
	strcpy(buffer, "\n");
	output(buffer);

	if (vtt.err(r)) {
		fprintf(stderr, "%s\n", vtt.err(r));
		vtt.freereader(r);
		return 1;
	}
	vtt.freereader(r);
	return 0;
}

char lastline[10000] = {};

void output(char *buf) {	
	char line[1000];
	while (true) {
		if (!popline(buf, line)) break;
		if (!strcmp(lastline, line)) {
			continue;
		}
		printf("%s\n", line);
		strcpy(lastline, line);
	}
}

// Copies the first complete line in buf into line and shifts
// the bytes in buf. Returns false if there is no \n character in buf.
bool popline(char *buf, char *line) {
	// Find where the first line ends.
	char *np = strchr(buf, '\n');
	if (!np) {
		return false;
	}

	// The first line length and the total buf length.
	size_t n = np - buf;
	size_t total = strlen(buf);

	// Copy the first line out.
	buf[n] = '\0';
	strcpy(line, buf);

	// Shift the buffer.
	memmove(buf, np + 1, total - n);
	return true;
}
