#import fs

const char *PATH = "fs-test.tmp";

bool test() {
	char *str = "test";
	size_t n = strlen(str);
	bool ok = fs.writefile(PATH, str, n);
	char *data = NULL;
	if (ok) {
		data = fs.readfile_str(PATH);
		ok = data != NULL;
	}
	if (ok) {
		ok = !strcmp(data, str);
	}
	fs.unlink(PATH);
	return ok;
}

int main() {
	if (test()) {
		return 0;
	}
	return 1;
}
