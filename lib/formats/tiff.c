pub typedef {
	int endiannes;
	FILE *file;
	size_t ndirs;
	dir_t **dirs;
} file_t;

pub typedef {
	size_t nentries;
	entry_t **entries;
} dir_t;

pub typedef {
	int tag;
	int type;
	size_t count;
	uint32_t value;
	file_t *file;
} entry_t;

// TIFF data types
pub enum {
	BYTE = 1,
	ASCII = 2,
	SHORT = 3,
	LONG = 4,
	RATIONAL = 5
}

// endiannes
enum {
	LITTLE = 0,
	BIG = 1
}

pub enum {
	ImageWidth = 256,
	ImageLength = 257,
	BitsPerSample = 258,
	Compression = 259,
	Uncompressed = 1,
	LZW = 5,
	ImageDescription = 270,
	StripOffsets = 273,
	RowsPerStrip = 278,
	StripByteCounts = 279
}

// Returns a display name for the given type.
pub const char *typename(int type) {
	switch (type) {
		case BYTE: { return "BYTE"; }
		case ASCII: { return "ASCII"; }
		case SHORT: { return "SHORT"; }
		case LONG: { return "LONG"; }
		case RATIONAL: { return "RATIONAL"; }
	}
	return "invalid type";
}

// Reads a TIFF file at the given path.
pub file_t *readfile(const char *filepath) {
	FILE *f = fopen(filepath, "rb");
	if (!f) {
		return NULL;
	}
	file_t *tf = calloc!(1, sizeof(file_t));
	tf->file = f;

	// Byte-order mark
	switch (readbytes(tf, 2)) {
		case 0x4949: { tf->endiannes = LITTLE; }
		case 0x4D4D: { tf->endiannes = BIG; }
		default: {
			panic("wrong endiannes marker");
		}
	}

	// Bytes 2-3 are TIFF magic number (42)
	if (readbytes(tf, 2) != 42) {
		panic("wrong magic number");
	}

	while (true) {
		// File offset. Zero means no more entries.
		uint32_t ifd_offset = readbytes(tf, 4);
		if (!ifd_offset) {
			break;
		}
		if (fseek(tf->file, ifd_offset, SEEK_SET) != 0) {
			panic("fseek %u failed", ifd_offset);
		}
		read_dir(tf);
	}
	return tf;
}

// Reads a directory at current file position
void read_dir(file_t *tiff) {
	// Number of entries in the directory.
	int entries_number = readbytes(tiff, 2);

	dir_t *dir = calloc!(1, sizeof(dir_t));
	dir->entries = calloc!(entries_number, sizeof( dir->entries[0] ));
	dir->nentries = entries_number;

	for (int entry_index = 0; entry_index < entries_number; entry_index++) {
		int tag = readbytes(tiff, 2); // bytes 0-1: the field tag
		int type = readbytes(tiff, 2); // 2-3: field type
		uint32_t count = readbytes(tiff, 4); // 4-7: number of values of the field
		uint32_t value = readbytes(tiff, 4); // 8-11: value offset (position of this field's value)
		
		entry_t *entry = calloc!(1, sizeof( entry_t ));
		entry->tag = tag;
		entry->type = type;
		entry->count = count;
		entry->value = value;
		entry->file = tiff;
		dir->entries[entry_index] = entry;
	}

	// Add the directory to the TIFF object.
	tiff->ndirs++;
	tiff->dirs = realloc(tiff->dirs, tiff->ndirs * sizeof(tiff->dirs[0]));
	tiff->dirs[tiff->ndirs-1] = dir;
}

// Frees all memory taken by the file and closes all handles.
pub void freefile(file_t *tiff) {
	for (size_t i = 0; i < tiff->ndirs; i++) {
		dir_t *dir = tiff->dirs[i];
		for (size_t j = 0; j < dir->nentries; j++) {
			free(dir->entries[j]);
		}
		free(dir->entries);
		free(dir);
	}
	free( tiff->dirs );
	fclose( tiff->file );
	free( tiff );
}

// Returns the string from the entry.
// The string must be freed by the caller.
pub char *getstring(entry_t *entry) {
	file_t *tiff = entry->file;
	char *buffer = calloc!(entry->count, 1);
	fseek(tiff->file, entry->value, SEEK_SET);
	for (size_t i = 0; i < entry->count; i++) {
		buffer[i] = readbytes(tiff, 1);
	}
	return buffer;
}

pub uint32_t readbytes(file_t *tiff, size_t bytes_number)
{
	uint32_t value = 0;
	uint8_t byte = 0;

	switch (tiff->endiannes) {
		case BIG: {
			for (size_t i = 0; i < bytes_number; i++) {
				fread( &byte, 1, 1, tiff->file );
				value = (value << 8) + byte;
			}
		}
		case LITTLE: {
			for (size_t i = 0; i < bytes_number; i++){
				fread( &byte, 1, 1, tiff->file );
				value += byte << 8*i;
			}
		}
	}
	return value;
}

pub const char *tagname(int tag) {
	switch (tag) {
		case 254: { return "NewSubfileType"; }
		case 255: { return "SubfileType"; }
		case 256: { return "ImageWidth"; }
		case 257: { return "ImageLength"; }
		case 258: { return "BitsPerSample"; }
		case 259: { return "Compression"; }

		case 262: { return "PhotometricInterpretation"; }

		case 266: { return "FillOrder"; }

		case 270: { return "ImageDescription"; }

		case 273: { return "StripOffsets"; }
		case 274: { return "Orientation"; }

		case 277: { return "SamplesPerPixel"; }
		case 278: { return "RowsPerStrip"; }
		case 279: { return "StripByteCounts"; }

		case 282: { return "XResolution"; }
		case 283: { return "YResolution"; }
		case 284: { return "PlanarConfiguration"; }

		case 296: { return "ResolutionUnit"; }

		case 305: { return "Software"; }
		case 306: { return "DateTime"; }

		case 315: { return "Artist"; }

		case 320: { return "ColorMap"; }
		default: {
			//printf( "Unknown tag: %d\n", tag );
			return "Unknown";
		}
	}
}
