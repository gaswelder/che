#import tiff

int usage() {
	puts("Usage: tiff <info|dump> <filename>");
	return 1;
}

int err(char *msg) {
	puts(msg);
	return 1;
}

int main(int argc, char *argv[])
{
	if (argc < 3) {
		return usage();
	}

	char *mode = argv[1];
	char *filepath = argv[2];

	TIFFFile *tiff = tiff_open( filepath );
	if( !tiff ){
		return err("Could not open the file");
	}

	if (strcmp(mode, "info") == 0) {
		int status = info(tiff);
		tiff_close( tiff );
		return status;
	}
	if (strcmp(mode, "dump") == 0) {
		int status = dump(tiff);
		tiff_close( tiff );
		return status;
	}
	tiff_close( tiff );
	return usage();
}

size_t tiff_dircount(TIFFFile *tiff) {
	return tiff->directories_count;
}

TIFFDirectory *tiff_dir(TIFFFile *tiff, size_t index) {
	return tiff->directories[index];
}

size_t tiff_entrycount(TIFFDirectory *dir) {
	return dir->entries_count;
}

TIFFEntry *tiff_entry(TIFFDirectory *dir, size_t index) {
	return dir->entries[index];
}


int info( TIFFFile *tiff )
{
	for (size_t dir_index = 0; dir_index < tiff_dircount(tiff); dir_index++) {
		printf( "-- Directory #%lu --\n", dir_index );
		TIFFDirectory *dir = tiff_dir(tiff, dir_index);

		for( size_t entry_index = 0; entry_index < tiff_entrycount(dir); entry_index++ )
		{
			TIFFEntry *entry = tiff_entry(dir, entry_index);
			printf("%-10s\t(x%lu)\t%-24s\t%u\n",
				tiff_typename(entry->type),
				entry->count,
				tiff_tagname(entry->tag),
				entry->value
			);

			// If the entry is a string, also print the contents.
			if (entry->type == TIFF_ASCII) {
				char *str = tiff_get_string(entry);
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

int dump( TIFFFile *tiff )
{
	TIFFDirectory *dir = tiff->directories[0];
	int width = 0;
	// int height = 0;
	int bits_per_sample = 0;
	int rows_per_strip = 0;
	int strips_per_image = 0;

	size_t *strip_offsets = NULL;
	size_t *strip_byte_counts = NULL;

	for( int i = 0; i < dir->entries_count; i++ )
	{
		TIFFEntry *entry = dir->entries[i];
		switch( entry->tag )
		{
			case TIFF_ImageWidth:
				width = entry->value;
				break;
			case TIFF_ImageLength:
				// height = entry->value;
				break;
			case TIFF_BitsPerSample:
				bits_per_sample = entry->value;
				break;
			case TIFF_RowsPerStrip:
				rows_per_strip = entry->value;
				break;
			case TIFF_StripOffsets:
				if( strips_per_image == 0 ){
					strips_per_image = entry->count;
				} else {
					if( entry->count != (size_t) strips_per_image ){
						fputs( "Inconsistent StripsPerImage value", stderr );
						exit( 2 );
					}
				}
				strip_offsets = malloc( sizeof( size_t ) * entry->count );
				fseek( tiff->file, entry->value, SEEK_SET );
				for( size_t j = 0; j < entry->count; j++ ){
					strip_offsets[j] = tiff_read_bytes( tiff, 4 );
				}
				break;
			case TIFF_StripByteCounts:
				if( strips_per_image == 0 ){
					strips_per_image = entry->count;
				} else {
					if( entry->count != (size_t) strips_per_image ){
						fputs( "Inconsistent StripsPerImage value", stderr );
						exit( 2 );
					}
				}
				strip_byte_counts = malloc( sizeof( size_t ) * entry->count );
				fseek( tiff->file, entry->value, SEEK_SET );
				for( size_t j = 0; j < entry->count; j++ ){
					strip_byte_counts[j] = tiff_read_bytes( tiff, 4 );
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
				printf( "%u ", tiff_read_bytes( tiff, bits_per_sample / 8 ) );
			}
			printf( "\n" );
		}
	}

	free( strip_offsets );
	free( strip_byte_counts );
	return 0;
}
