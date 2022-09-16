#import fileutil
#import os/fs

const char *PATH = "fileutil-test.tmp";

bool test() {
	char *str = "test";
	size_t n = strlen(str);
	bool ok = fileutil.writefile(PATH, str, n);
	char *data = NULL;
	if (ok) {
		data = readfile_str(PATH);
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
