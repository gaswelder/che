#import os/fs

pub typedef {
	int endiannes;
	size_t ndirs;
	dir_t **dirs;

	uint8_t *data;
	size_t datalen;
	size_t pos;
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

// Parses data as a TIFF file.
pub file_t *parse(uint8_t *data, size_t n) {
	file_t *f = calloc!(1, sizeof(file_t));
	f->data = data;
	f->datalen = n;
	// Byte-order mark
	switch (readbytes(f, 2)) {
		case 0x4949: { f->endiannes = LITTLE; }
		case 0x4D4D: { f->endiannes = BIG; }
		default: { panic("wrong endiannes marker"); }
	}
	// Bytes 2-3 are TIFF magic number (42)
	if (readbytes(f, 2) != 0x2a) {
		panic("wrong magic number");
	}
	while (true) {
		// File offset. Zero means no more entries.
		uint32_t pos = readbytes(f, 4);
		if (!pos) {
			break;
		}
		f->pos = pos;
		read_dir(f);
	}
	return f;
}

// Reads a TIFF file at the given path.
pub file_t *readfile(const char *filepath) {
	size_t flen;
	char *data = fs.readfile(filepath, &flen);
	if (!data) {
		return NULL;
	}
	return parse((uint8_t *) data, flen);
}

pub void setpos(file_t *f, uint32_t pos) {
	f->pos = pos;
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
pub void freefile(file_t *tf) {
	for (size_t i = 0; i < tf->ndirs; i++) {
		dir_t *dir = tf->dirs[i];
		for (size_t j = 0; j < dir->nentries; j++) {
			free(dir->entries[j]);
		}
		free(dir->entries);
		free(dir);
	}
	free(tf->dirs);
	free(tf->data);
	free(tf);
}

// Returns the string from the entry.
// The string must be freed by the caller.
pub char *getstring(entry_t *e) {
	file_t *tf = e->file;
	char *buffer = calloc!(e->count + 1, 1);
	tf->pos = e->value;
	for (size_t i = 0; i < e->count; i++) {
		buffer[i] = (char) tf->data[tf->pos++];
	}
	return buffer;
}

// Reads n bytes at the current position.
pub uint32_t readbytes(file_t *tf, size_t n) {
	uint32_t val = 0;
	switch (tf->endiannes) {
		case BIG: {
			for (size_t i = 0; i < n; i++) {
				val *= 256;
				val += tf->data[tf->pos++];
			}
		}
		case LITTLE: {
			uint32_t scale = 1;
			for (size_t i = 0; i < n; i++){
				val += tf->data[tf->pos++] * scale;
				scale *= 256;
			}
		}
		default: {
			panic("invalid endiannes");
		}
	}
	return val;
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
