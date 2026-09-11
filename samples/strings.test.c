
int main() {
	const char *s = "multiline
string";
	if (strcmp(s, "multiline\nstring") != 0) panic("!");

	const char *s2 = "\x41" "foo";
	if (strcmp(s2, "Afoo") != 0) panic("!");
	return 0;
}
