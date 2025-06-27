#import os/fs
#import test

int main() {
	const char *PATH = "fs-test.tmp";
	const char *str = "test";
	size_t n = strlen(str);

	test.truth("writefile", fs.writefile(PATH, str, n));


	char *data = fs.readfile_str(PATH);
	test.truth("readfile_str", data != NULL);

	test.truth("strcmp", strcmp(data, str) == 0);

	test.truth("unlink", fs.unlink(PATH));

	return test.fails();
}
