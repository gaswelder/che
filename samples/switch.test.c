int main() {
	if (classify("foo") != 123) panic("!");
	if (classify("kek") != 123) panic("!");
	if (classify("bar") != 456) panic("!");
	if (classify("none") != 2) panic("!");
	return 0;
}

int classify(char *s) {
	switch str (s) {
		case "foo", "kek": { return 123; }
		case "bar": { return 456; }
		default: { return 2; }
	}
}
