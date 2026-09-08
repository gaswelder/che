int main(int argc, char *argv[]) {
	for (int i = 1; i < argc; i++) {
		print(argv[i]);
	}
	return 0;
}

void print(char *s) {
	switch str (s) {
		case "foo": { puts("ok, foo"); }
		case "bar": { puts("ok, bar"); }
		default: { puts("neither"); }
	}
}
