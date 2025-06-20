
int main(int argc, char **argv) {
	for (int i = 1; i < argc; i++) {
		char *s = argv[i];
		char *d = dot(s);
		if (d == s) {
			puts(s);
			continue;
		}
		char *c = s;
		while (*c != '\0' && c != d) {
			putchar(*c++);
		}
		putchar('\n');
	}

	return 0;
}

char *dot(char *s) {
	char *c = s;
	while (*c != '\0') c++;
	while (c != s && *c != '.') c--;
	return c;
}
