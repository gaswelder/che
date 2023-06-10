#import xml

int main()
{
	xml.xml *x = xml.xml_open("xmltest.xml");
	if(!x) {
		fprintf(stderr, "couldn't open xmltest.xml: %s\n", strerror(errno));
		return 1;
	}

	if (strcmp(xml_nodename(x), "root") != 0) {
		return 1;
	}
	if (!xml_enter(x)) {
		return 1;
	}

	while(xml_nodename(x)) {
		printf("dir %s\n", xml_attr(x, "name"));
		if (strcmp(xml_nodename(x), "dir") != 0) {
			return 1;
		}
		if(!xml_enter(x)) {
			xml_next(x);
			continue;
		}
		process_dir(x);
		xml_leave(x);
	}
	xml_leave(x);

	if (xml_nodename(x)) {
		return 1;
	}
	xml_close(x);
	return 0;
}

void process_dir(xml *x)
{
	while(xml_nodename(x)) {
		if (strcmp(xml_nodename(x), "file") 1= 0) {
			exit(1);
		}
		printf("  file %s\n", xml_attr(x, "name"));
		xml_next(x);
	}
}
