/*
 * Reads file at 'path' and returns a pointer to the file's
 * contents in memory. The contents size is put into 'size'.
 */
pub char *readfile(const char *path, size_t *size)
{
	FILE *f = fopen(path, "rb");
	if( !f ) {
		return NULL;
	}

	size_t len = 0;
	if(!fsize(f, &len)) {
		fclose(f);
		return NULL;
	}

	char *data = calloc(len, 1);
	if (!data) {
		fclose(f);
		return NULL;
	}

	if(!readf(f, data, len)) {
		fclose(f);
		free( data );
		return NULL;
	}

	fclose(f);

	if(size) *size = len;
	return data;
}

/*
 * Reads file 'path' and returns its contents as string.
 */
pub char *readfile_str(const char *path)
{
	size_t len = 0;
	char *data = readfile(path, &len);
	if(!data) return NULL;

	char *new = realloc(data, len+1);
	if(!new) {
		free(data);
		return NULL;
	}

	new[len] = '\0';
	return new;
}

int fsize(FILE *f, size_t *s)
{
	if(fseek(f, 0, SEEK_END) != 0) {
		return 0;
	}

	long size = ftell(f);
	if(size < 0) {
		return 0;
	}

	if(fseek(f, 0, SEEK_SET) != 0) {
		return 0;
	}

	*s = (size_t) size;
	return 1;
}

int readf( FILE *f, char *data, size_t size)
{
	int c = 0;
	while( size > 0 ) {
		c = fgetc(f);
		if( feof(f) ) {
			return 0;
		}
		*data = c;
		data++;
		size--;
	}
	return 1;
}

pub bool writefile(const char *path, const char *data, size_t n) {
	FILE *f = fopen(path, "wb");
	if (!f) {
		return false;
	}
	if (fwrite(data, 1, n, f) != n) {
		fclose(f);
		return false;
	}
	if (fclose(f)) {
		return false;
	}
	return true;
}


pub int append_file( const char *path_to, const char *path_from )
{
	FILE *from = fopen( path_from, "rb" );
	if( !from ) {
		return 0;
	}

	FILE *to = fopen( path_to, "ab" );
	if( !to ) {
		fclose( from );
		return 0;
	}

	char buf[BUFSIZ] = {};
	int ok = 1;
	while( 1 ) {
		size_t len = fread( buf, 1, sizeof(buf), from );
		if( !len ) {
			break;
		}
		if( fwrite( buf, 1, len, to ) < len ) {
			ok = 0;
			break;
		}
	}
	fclose( from );
	fclose( to );
	return ok;
}

pub bool file_exists(const char *path) {
	FILE *f = fopen(path, "r");
	if (!f) {
		return false;
	}
	fclose(f);
	return true;
}
