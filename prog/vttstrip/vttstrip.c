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

bool popline(char *buf, char *line) {
	char *np = strchr(buf, '\n');
	if (!np) return false;
	size_t n = np - buf;
	size_t total = strlen(buf);
	buf[n] = '\0';
	strcpy(line, buf);
	memmove(buf, np + 1, total - n);
	return true;
}
