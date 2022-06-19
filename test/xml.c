#import xml

int main()
{
	xml *x = xml_open("xmltest.xml");
	if(!x) {
		fprintf(stderr, "couldn't open xmltest.xml: %s\n", strerror(errno));
		return 1;
	}

	assert(strcmp(xml_nodename(x), "root") == 0);
	assert(xml_enter(x));

	while(xml_nodename(x)) {
		printf("dir %s\n", xml_attr(x, "name"));
		assert(strcmp(xml_nodename(x), "dir") == 0);
		if(!xml_enter(x)) {
			xml_next(x);
			continue;
		}
		process_dir(x);
		xml_leave(x);
	}
	xml_leave(x);

	assert(!xml_nodename(x));
	xml_close(x);
}

void process_dir(xml *x)
{
	while(xml_nodename(x)) {
		assert(strcmp(xml_nodename(x), "file") == 0);
		printf("  file %s\n", xml_attr(x, "name"));
		xml_next(x);
	}
}
