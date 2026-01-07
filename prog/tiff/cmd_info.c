#import formats/tiff
#import formats/json

pub int run(int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, "arguments: <filepath>\n");
		return 1;
	}
	char *filepath = argv[1];
	tiff.file_t *tf = tiff.readfile(filepath);
	if (!tf) {
		fprintf(stderr, "Could not open %s: %s\n", filepath, strerror(errno));
		return 1;
	}
	printfile(tf);
	tiff.freefile(tf);
	return 0;
}

void printfile(tiff.file_t *f) {
	for (size_t i = 0; i < f->ndirs; i++) {
		printdir(f->dirs[i]);
		printf("\n");
	}
}

void printdir(tiff.dir_t *dir) {
	printf("{");
	json.write_string(stdout, "entries");
	printf(":[");
	for (size_t i = 0; i < dir->nentries; i++) {
		if (i > 0) printf(",");
		printentry(dir->entries[i]);
	}
	printf("]}");
}

void printentry(tiff.entry_t *entry) {
	printf("{");
	json.write_string(stdout, "type");
	printf(":");
	json.write_string(stdout, tiff.typename(entry->type));
	printf(",");

	json.write_string(stdout, "tag");
	printf(":");
	json.write_string(stdout, tiff.tagname(entry->tag));
	printf(",");
	
	json.write_string(stdout, "value");
	printf(":");
	switch (entry->type) {
		case tiff.BYTE, tiff.SHORT, tiff.LONG: {
			printf("%u", entry->value);
		}
		case tiff.RATIONAL: {
			printf("%u", entry->value);
		}
		case tiff.ASCII: {
			char *str = tiff.getstring(entry);
			if (!str) {
				panic("failed to get ASCIII");
			}
			json.write_string(stdout, str);
			free(str);
		}
	}
	printf("}");
}
