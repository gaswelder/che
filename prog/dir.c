import "os/dir"

int main(int argc, char *argv[])
{
	if(argc > 2) {
		fprintf(stderr, "Usage: dir [path]\n");
		return 1;
	}

	const char *path = NULL;
	if(argc > 1) {
		path = argv[1];
	}
	else {
		path = ".";
	}

	dir_t *d = diropen(path);
	if(!d) {
		fprintf(stderr, "Couldn't read %s\n", path);
		return 1;
	}

	while (true) {
		const char *fn = dirnext(d);
		if (!fn) break;
		printf("%s\n", fn);
	}

	dirclose(d);

	return 0;
}
