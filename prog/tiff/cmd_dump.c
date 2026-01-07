#import formats/tiff

pub int run(int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, "arguments: <filepath>\n");
		return 1;
	}
	char *filepath = argv[1];
	tiff.TIFFFile *tiff = tiff.tiff_open(filepath);
	if (!tiff) {
		fprintf(stderr, "Could not open %s: %s\n", filepath, strerror(errno));
		return 1;
	}
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
		switch (entry->tag) {
			case tiff.TIFF_ImageWidth: {
				width = entry->value;
			}
			case tiff.TIFF_ImageLength: {
				// height = entry->value;
			}
			case tiff.TIFF_BitsPerSample: {
				bits_per_sample = entry->value;
			}
			case tiff.TIFF_RowsPerStrip: {
				rows_per_strip = entry->value;
			}
			case tiff.TIFF_StripOffsets: {
				if( strips_per_image == 0 ){
					strips_per_image = entry->count;
				} else {
					if( entry->count != (size_t) strips_per_image ){
						fputs( "Inconsistent StripsPerImage value", stderr );
						exit( 2 );
					}
				}
				strip_offsets = calloc!(entry->count, sizeof(size_t));
				fseek( tiff->file, entry->value, SEEK_SET );
				for( size_t j = 0; j < entry->count; j++ ){
					strip_offsets[j] = tiff.tiff_read_bytes( tiff, 4 );
				}
			}
			case tiff.TIFF_StripByteCounts: {
				if( strips_per_image == 0 ){
					strips_per_image = entry->count;
				} else {
					if( entry->count != (size_t) strips_per_image ){
						fputs( "Inconsistent StripsPerImage value", stderr );
						exit( 2 );
					}
				}
				strip_byte_counts = calloc!(entry->count, sizeof(size_t));
				fseek( tiff->file, entry->value, SEEK_SET );
				for( size_t j = 0; j < entry->count; j++ ){
					strip_byte_counts[j] = tiff.tiff_read_bytes( tiff, 4 );
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
	tiff.tiff_close( tiff );
	return 0;
}
