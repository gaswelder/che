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

	size_t len;
	if(!fsize(f, &len)) {
		fclose(f);
		return NULL;
	}

	char *data = malloc(len);
	if(!data) {
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
	size_t len;
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

	ssize_t size = (ssize_t) ftell(f);
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
	int c;
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
