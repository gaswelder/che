// License: MIT (https://opensource.org/licenses/MIT)

int strput(char *dest, size_t destsize, const char *src)
{
	if( !src || strlen(src) + 1 > destsize ) {
		return 0;
	}

	strcpy(dest, src);
	return 1;
}

/*
 * Creates a new string in memory, formatted printf-style according
 * to the given arguments. The returned string must be freed by the
 * caller.
 */
char *newstr( const char *format, ... )
{
	va_list args;

	/*
	 * Buffer with default size.
	 */
	size_t bufsize = 1;
	char *buf = malloc( bufsize );
	if( !buf ) {
		return NULL;
	}

	while( 1 ) {
		va_start( args, format );
		int r = vsnprintf( buf, bufsize, format, args );
		va_end( args );
		/*
		 * If there is a format error, abort
		 */
		if( r < 0 ) {
			free( buf );
			return NULL;
		}

		/*
		 * If the string turns out to be larger than the buffer,
		 * resize the buffer and try again.
		 */
		if( (size_t) r >= bufsize ) {
			char *tmp = realloc( buf, r + 1 );
			if( !tmp ) {
				free( buf );
				return NULL;
			}
			buf = tmp;
			bufsize = r + 1;
			continue;
		}
		break;
	}

	return buf;
}
