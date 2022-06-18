
// TIFF data types
pub enum {
	TIFF_BYTE = 1,
	TIFF_ASCII = 2,
	TIFF_SHORT = 3,
	TIFF_LONG = 4,
	TIFF_RATIONAL = 5
};

// endiannes
enum {
	LITTLE = 0,
	BIG = 1
};

pub enum {
	TIFF_ImageWidth = 256,
	TIFF_ImageLength = 257,
	TIFF_BitsPerSample = 258,
	TIFF_Compression = 259,
	TIFF_Uncompressed = 1,
	TIFF_LZW = 5,
	TIFF_ImageDescription = 270,
	TIFF_StripOffsets = 273,
	TIFF_RowsPerStrip = 278,
	TIFF_StripByteCounts = 279
};

pub typedef {
	int tag;
	int type;
	size_t count;
	uint32_t value;
	TIFFFile *file;
} TIFFEntry;

pub typedef {
	TIFFEntry **entries;
	int entries_count;
} TIFFDirectory;

pub typedef {
	int endiannes;
	FILE *file;
	TIFFDirectory **directories;
	int directories_count;
} TIFFFile;

typedef {
	int k;
	const char *v;
} kv;

kv typenames[] = {
	{TIFF_BYTE, "BYTE"},
	{TIFF_ASCII, "ASCII"},
	{TIFF_SHORT, "SHORT"},
	{TIFF_LONG, "LONG"},
	{TIFF_RATIONAL, "RATIONAL"}		
};

pub const char *tiff_typename(int type)
{
	for (size_t i = 0; i < sizeof(typenames) / sizeof(typenames[0]); i++) {
		kv entry = typenames[i];
		if (entry.k == type) {
			return entry.v;
		}
	}
	return "UNKNOWN";
}

TIFFEntry *TIFFEntry_Create( int tag, int type, uint32_t count, uint32_t value, TIFFFile *file )
{
	TIFFEntry *entry = calloc( 1, sizeof( TIFFEntry ) );
	entry->tag = tag;
	entry->type = type;
	entry->count = count;
	entry->value = value;
	entry->file = file;
	return entry;
}

TIFFDirectory *TIFFDirectory_Create( size_t entries_number )
{
	TIFFDirectory *dir = calloc( 1, sizeof( TIFFDirectory ) );
	dir->entries = calloc( entries_number, sizeof( dir->entries[0] ) );
	dir->entries_count = entries_number;
	return dir;
}

void TIFFDirectory_Destroy( TIFFDirectory *dir )
{
	for( int i = 0; i < dir->entries_count; i++ ){
		free( dir->entries[i] );
	}
	free( dir->entries );
	free( dir );
}


//
// Adds given directory to given TIFF object
//
void TIFFFile_AddDirectory( TIFFFile *tiff, TIFFDirectory *dir )
{
	//
	// Increase directories counter,
	// allocate memory for new reference to the given object,
	// put the reference in the directories list (array).
	//
	tiff->directories_count++;
	tiff->directories = realloc(
		tiff->directories,
		tiff->directories_count * sizeof( tiff->directories[0] )
	);
	tiff->directories[tiff->directories_count-1] = dir;
}

//
// Opens a file with given name
// and reads its contents into a new TIFF object.
//
pub TIFFFile *tiff_open( const char *filepath )
{
	//
	// Create a TIFF object
	//
	TIFFFile *tiff = calloc( 1, sizeof( TIFFFile ) );
	
	//
	// Open the file and store its descriptor
	// in the TIFF object
	//
	tiff->file = fopen( filepath, "rb" );
	if( !tiff->file ){
		fprintf( stderr, "Could not open file '%s'\n", filepath );
		free( tiff );
		return NULL;
	}

	//
	// First two bytes in the file are byte-order mark
	//
	switch( tiff_read_bytes( tiff, 2 ) )
	{
		case 0x4949:
			tiff->endiannes = LITTLE;
			break;
		case 0x4D4D:
			tiff->endiannes = BIG;
			break;
		default:
			fprintf( stderr, "Error: wrong endiannes marker in the header\n" );
			tiff_close( tiff );
			return NULL;
	}

	
	//
	// Bytes 2-3 are TIFF magic number (42)
	//
	if( tiff_read_bytes( tiff, 2 ) != 42 ){
		fprintf( stderr, "Error: wrong magic number\n" );
		tiff_close( tiff );
		return NULL;
	}

	//
	// Start parsing directory entries
	// * uint32_t is an integer (position in file)
	//
	// 1. Read offset at current file position
	// 2. If the offset is zero, stop.
	// 3. Jump to the offset.
	// 4. Read the directory that is placed at that new position
	// 5. Go to 1.
	//
	uint32_t ifd_offset = tiff_read_bytes( tiff, 4 );
	while( ifd_offset )
	{
		//printf( "Jumping to IFD at position %u\n", ifd_offset );
		if( fseek( tiff->file, ifd_offset, SEEK_SET ) ){
			fprintf( stderr, "Could not read the file at position %u\n", ifd_offset );
			tiff_close( tiff );
			return NULL;
		}
		
		//
		// Read the directory and read
		// the next offset after the directory
		//
		parse_ifd( tiff );
		ifd_offset = tiff_read_bytes( tiff, 4 );
		//if( !ifd_offset ) puts( "No more IFD's" );
	}

	return tiff;
}


//
// Reads a directory at current file position
//
void parse_ifd( TIFFFile *tiff )
{
	//
	// 2 bytes - number of entries in the directory
	//
	int entries_number = tiff_read_bytes( tiff, 2 );
	//printf( "There are %u entries\n", entries_number );

	//
	// Create a directory object
	TIFFDirectory *dir = TIFFDirectory_Create( entries_number );

	// Read N directory entries, where N is known
	for( int entry_index = 0; entry_index < entries_number; entry_index++ )
	{
		// bytes 0-1: the field tag
		int tag = tiff_read_bytes( tiff, 2 );
		// 2-3: field type
		int type = tiff_read_bytes( tiff, 2 );
		// 4-7: number of values of the field
		uint32_t count = tiff_read_bytes( tiff, 4 );
		// 8-11: value offset (position of this field's value)
		uint32_t value = tiff_read_bytes( tiff, 4 );

		//
		// Create TIFF entry object with these parameters
		// and add it to the directory
		//
		TIFFEntry *entry = TIFFEntry_Create( tag, type, count, value, tiff );
		dir->entries[entry_index] = entry;

		//printf( "%-10s x %-5d %-30s %u\n", typename( type ), count, tagname( tag ), value );
	}

	//
	// Add the directory to the TIFF object
	//
	TIFFFile_AddDirectory( tiff, dir );
}

//
// Frees all memory taken by the TIFF and closes all handles
//
pub void tiff_close( TIFFFile *tiff )
{
	if( tiff == NULL ){
		return;
	}

	//
	// Free each directory from this TIFF
	//
	for( int i = 0; i < tiff->directories_count; i++ ){
		TIFFDirectory_Destroy( tiff->directories[i] );
	}
	free( tiff->directories );
	fclose( tiff->file );
	free( tiff );
}


/*
 * Returns the string from an ASCII entry.
 * The string must be freed by the caller.
 */
pub char *tiff_get_string(TIFFEntry *entry)
{
	char *buffer = calloc(entry->count, 1);
	if (!buffer) {
		return NULL;
	}

	TIFFFile *tiff = entry->file;

	fseek( tiff->file, entry->value, SEEK_SET );
	for (size_t i = 0; i < entry->count; i++) {
		buffer[i] = tiff_read_bytes(tiff, 1);
	}
	return buffer;
}

pub uint32_t tiff_read_bytes(TIFFFile *tiff, size_t bytes_number)
{
	uint32_t value = 0;
	uint8_t byte = 0;

	switch( tiff->endiannes )
	{
		case BIG:
			for( size_t i = 0; i < bytes_number; i++ ){
				fread( &byte, 1, 1, tiff->file );
				value = (value << 8) + byte;
			}
			break;

		case LITTLE:
			for( size_t i = 0; i < bytes_number; i++ ){
				fread( &byte, 1, 1, tiff->file );
				value += byte << 8*i;
			}
			break;
	}
	return value;
}

pub const char *tiff_tagname( int tag )
{
	switch( tag )
	{
		case 254: return "NewSubfileType";
		case 255: return "SubfileType";
		case 256: return "ImageWidth";
		case 257: return "ImageLength";
		case 258: return "BitsPerSample";
		case 259: return "Compression";

		case 262: return "PhotometricInterpretation";

		case 266: return "FillOrder";

		case 270: return "ImageDescription";

		case 273: return "StripOffsets";
		case 274: return "Orientation";

		case 277: return "SamplesPerPixel";
		case 278: return "RowsPerStrip";
		case 279: return "StripByteCounts";

		case 282: return "XResolution";
		case 283: return "YResolution";
		case 284: return "PlanarConfiguration";

		case 296: return "ResolutionUnit";

		case 305: return "Software";
		case 306: return "DateTime";

		case 315: return "Artist";

		case 320: return "ColorMap";
		default:
			//printf( "Unknown tag: %d\n", tag );
			return "Unknown";
	}
}

