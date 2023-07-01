#import formats/tiff

void usage() {
	fprintf(stderr, "Usage: tiff <info|dump> <filename>\n");
}

int main(int argc, char *argv[]) {
	if (argc < 3) {
		usage();
		return 1;
	}

	char *mode = argv[1];
	char *filepath = argv[2];

	tiff.TIFFFile *tiff = tiff.tiff_open( filepath );
	if (!tiff) {
		fprintf(stderr, "Could not open %s: %s\n", filepath, strerror(errno));
		return 1;
	}

	if (strcmp(mode, "info") == 0) {
		int status = info(tiff);
		tiff.tiff_close( tiff );
		return status;
	}
	if (strcmp(mode, "dump") == 0) {
		int status = dump(tiff);
		tiff.tiff_close( tiff );
		return status;
	}
	tiff.tiff_close( tiff );
	usage();
	return 1;
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


int info( tiff.TIFFFile *tiff ) {
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
			if (entry->type == tiff.TIFF_ASCII) {
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
	return 0;
}

int dump( tiff.TIFFFile *tiff )
{
	tiff.TIFFDirectory *dir = tiff->directories[0];
	int width = 0;
	// int height = 0;
	int bits_per_sample = 0;
	int rows_per_strip = 0;
	int strips_per_image = 0;

	size_t *strip_offsets = NULL;
	size_t *strip_byte_counts = NULL;

	for( int i = 0; i < dir->entries_count; i++ )
	{
		tiff.TIFFEntry *entry = dir->entries[i];
		switch( entry->tag )
		{
			case tiff.TIFF_ImageWidth:
				width = entry->value;
				break;
			case tiff.TIFF_ImageLength:
				// height = entry->value;
				break;
			case tiff.TIFF_BitsPerSample:
				bits_per_sample = entry->value;
				break;
			case tiff.TIFF_RowsPerStrip:
				rows_per_strip = entry->value;
				break;
			case tiff.TIFF_StripOffsets:
				if( strips_per_image == 0 ){
					strips_per_image = entry->count;
				} else {
					if( entry->count != (size_t) strips_per_image ){
						fputs( "Inconsistent StripsPerImage value", stderr );
						exit( 2 );
					}
				}
				strip_offsets = calloc(entry->count, sizeof(size_t));
				fseek( tiff->file, entry->value, SEEK_SET );
				for( size_t j = 0; j < entry->count; j++ ){
					strip_offsets[j] = tiff.tiff_read_bytes( tiff, 4 );
				}
				break;
			case tiff.TIFF_StripByteCounts:
				if( strips_per_image == 0 ){
					strips_per_image = entry->count;
				} else {
					if( entry->count != (size_t) strips_per_image ){
						fputs( "Inconsistent StripsPerImage value", stderr );
						exit( 2 );
					}
				}
				strip_byte_counts = calloc(entry->count, sizeof(size_t));
				fseek( tiff->file, entry->value, SEEK_SET );
				for( size_t j = 0; j < entry->count; j++ ){
					strip_byte_counts[j] = tiff.tiff_read_bytes( tiff, 4 );
				}
				break;
		}
	}

	if( !strip_offsets || !strip_byte_counts ){
		fputs( "Missing StripOffsets or StripByteCounts", stderr );
		return 2;
	}

	for (int i = 0; i < strips_per_image; i++)
	{
		// go to the next strip
		if( fseek( tiff->file, strip_offsets[i], SEEK_SET ) ){
			fprintf( stderr, "Could not SEEK_SET position %lu\n", strip_offsets[i] );
			return 2;
		}

		for( int row = 0; row < rows_per_strip; row++ )
		{
			for( int col = 0; col < width; col++ ){
				printf( "%u ", tiff.tiff_read_bytes( tiff, bits_per_sample / 8 ) );
			}
			printf( "\n" );
		}
	}

	free( strip_offsets );
	free( strip_byte_counts );
	return 0;
}
