#import xml.c
#import fileutil
#import test
#import os/fs

const char *data = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<root>\n"
"	<dir name=\"dir1\">\n"
"		<file name=\"file1\" />\n"
"		<file name=\"file2\" />\n"
"	</dir>\n"
"	<dir name=\"dir3\" />\n"
"</root>\n";

const char *TEMP_FILE_PATH = "xmltest-tmp.xml";

int main() {
	if (!fileutil.writefile(TEMP_FILE_PATH, data, strlen(data))) {
		panic("failed to create the test file");
	}

	xml.xml *x = xml.xml_open(TEMP_FILE_PATH);
	if (!x) {
		panic("couldn't open xmltest.xml: %s", strerror(errno));
	}

	test.streq(xml.xml_nodename(x), "root");
	test.truth("enter", xml.xml_enter(x));

	// first dir
	test.streq(xml.xml_nodename(x), "dir");
	test.streq(xml.xml_attr(x, "name"), "dir1");
	xml.xml_enter(x);

		test.streq(xml.xml_nodename(x), "file");
		test.streq(xml.xml_attr(x, "name"), "file1");
		xml.xml_next(x);

		test.streq(xml.xml_nodename(x), "file");
		test.streq(xml.xml_attr(x, "name"), "file2");
		xml.xml_next(x);

		test.truth("!nodename", !xml.xml_nodename(x));
	xml.xml_leave(x);

	// second dir
	test.streq(xml.xml_nodename(x), "dir");
	test.streq(xml.xml_attr(x, "name"), "dir3");
	test.truth("second dir is empty", !xml.xml_enter(x));

	// end of dirs
	xml.xml_next(x);
	test.truth("expect empty nodename", !xml.xml_nodename(x));
	xml.xml_close(x);
	fs.unlink(TEMP_FILE_PATH);
	return test.fails();
}
