#import formats/tiff

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
	tiff.dir_t *dir = tf->dirs[0];
	int width = 0;
	// int height = 0;
	int bits_per_sample = 0;
	int rows_per_strip = 0;
	int strips_per_image = 0;

	size_t *strip_offsets = NULL;
	size_t *strip_byte_counts = NULL;

	for (size_t i = 0; i < dir->nentries; i++) {
		tiff.entry_t *entry = dir->entries[i];
		switch (entry->tag) {
			case tiff.ImageWidth: {
				width = entry->value;
			}
			case tiff.ImageLength: {
				// height = entry->value;
			}
			case tiff.BitsPerSample: {
				bits_per_sample = entry->value;
			}
			case tiff.RowsPerStrip: {
				rows_per_strip = entry->value;
			}
			case tiff.StripOffsets: {
				if( strips_per_image == 0 ){
					strips_per_image = entry->count;
				} else {
					if( entry->count != (size_t) strips_per_image ){
						fputs( "Inconsistent StripsPerImage value", stderr );
						exit( 2 );
					}
				}
				strip_offsets = calloc!(entry->count, sizeof(size_t));
				fseek(tf->file, entry->value, SEEK_SET );
				for( size_t j = 0; j < entry->count; j++ ){
					strip_offsets[j] = tiff.readbytes(tf, 4);
				}
			}
			case tiff.StripByteCounts: {
				if( strips_per_image == 0 ){
					strips_per_image = entry->count;
				} else {
					if( entry->count != (size_t) strips_per_image ){
						fputs( "Inconsistent StripsPerImage value", stderr );
						exit( 2 );
					}
				}
				strip_byte_counts = calloc!(entry->count, sizeof(size_t));
				fseek(tf->file, entry->value, SEEK_SET );
				for( size_t j = 0; j < entry->count; j++ ){
					strip_byte_counts[j] = tiff.readbytes(tf, 4);
				}
			}
		}
	}

	if( !strip_offsets || !strip_byte_counts ){
		fputs( "Missing StripOffsets or StripByteCounts", stderr );
		return 2;
	}

	for (int i = 0; i < strips_per_image; i++)
	{
		// go to the next strip
		if (fseek(tf->file, strip_offsets[i], SEEK_SET )) {
			fprintf( stderr, "Could not SEEK_SET position %lu\n", strip_offsets[i] );
			return 2;
		}

		for( int row = 0; row < rows_per_strip; row++ ) {
			for (int col = 0; col < width; col++) {
				printf("%u ", tiff.readbytes(tf, bits_per_sample / 8 ) );
			}
			printf( "\n" );
		}
	}

	free( strip_offsets );
	free( strip_byte_counts );
	tiff.freefile(tf);
	return 0;
}
