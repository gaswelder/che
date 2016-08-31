// License: MIT (https://opensource.org/licenses/MIT)

int writefile( const char *path, const char *data, size_t len )
{
	FILE *f = fopen( path, "wb" );
	if( !f ) {
		return 0;
	}

	while( len > 0 )
	{
		int c = (int)( (unsigned char) *data );
		if( fputc( *data, f ) != c ) {
			fclose(f);
			return 0;
		}
		len--;
	}

	fclose(f);
	return 1;
}

char *readfile(const char *path)
{
	FILE *f = fopen(path, "rb");
	if( !f ) return NULL;

	long int size = fsize(f);
	if( size == -1L ) {
		fclose(f);
		return NULL;
	}

	char *data = malloc((size_t) size);
	if( !data ) {
		fclose(f);
		return NULL;
	}

	if( !readf( f, data, size ) ) {
		fclose(f);
		free( data );
		return NULL;
	}

	fclose(f);
	return data;
}

int file_exists( const char *path )
{
	FILE *f = fopen( path, "r" );
	if( !f ) return 0;
	fclose( f );
	return 1;
}

int append_file( const char *path_to, const char *path_from )
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

	char buf[BUFSIZ];
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

static long int fsize( FILE *f )
{
	if( fseek(f, 0, SEEK_END) != 0 ) {
		return -1L;
	}

	long int size = ftell(f);
	if( size == -1L ) {
		return -1L;
	}

	if( fseek(f, 0, SEEK_SET) != 0 ) {
		return -1L;
	}

	return size;
}

static int readf( FILE *f, char *data, long int size )
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
