
int main() {
	const char *s = "multiline
string";
	if (strcmp(s, "multiline\nstring") != 0) {
		panic("!");
	}
	return 0;
}
