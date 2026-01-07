#import formats/tiff

pub int run(int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, "arguments: <filepath>\n");
		return 1;
	}
	char *filepath = argv[1];
	tiff.TIFFFile *tiff = tiff.tiff_open( filepath );
	if (!tiff) {
		fprintf(stderr, "Could not open %s: %s\n", filepath, strerror(errno));
		return 1;
	}
	for (int dir_index = 0; dir_index < tiff->directories_count; dir_index++) {
		printf( "-- Directory #%d --\n", dir_index);
		tiff.TIFFDirectory *dir = tiff_dir(tiff, dir_index);

		for (size_t entry_index = 0; entry_index < tiff_entrycount(dir); entry_index++) {
			tiff.TIFFEntry *entry = tiff_entry(dir, entry_index);
			printf("%-10s\t(x%lu)\t%-24s\t%u\n",
				tiff.tiff_typename(entry->type),
				entry->count,
				tiff.tiff_tagname(entry->tag),
				entry->value
			);

			// If the entry is a string, also print the contents.
			if (entry->type == tiff.ASCII) {
				char *str = tiff.tiff_get_string(entry);
				if (!str) {
					fprintf(stderr, "failed to get ASCII value\n");
					continue;
				}
				printf("\t%s\n\n", str);
				free(str);
			}
		}
	}
	tiff.tiff_close( tiff );
	return 0;
}

tiff.TIFFDirectory *tiff_dir(tiff.TIFFFile *tiff, size_t index) {
	return tiff->directories[index];
}

size_t tiff_entrycount(tiff.TIFFDirectory *dir) {
	return dir->entries_count;
}

tiff.TIFFEntry *tiff_entry(tiff.TIFFDirectory *dir, size_t index) {
	return dir->entries[index];
}
