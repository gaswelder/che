
int err( const char *fmt, ... )
{
	va_list args;
	int r;

	va_start( args, fmt );
	r = vfprintf( stderr, fmt, args );
	if( r >= 0 && fputc( '\n', stderr ) == '\n' ) {
		r++;
	}
	va_end( args );
	return r;
}

int warn( const char *fmt, ... )
{
	va_list args;
	int r;

	va_start( args, fmt );
	r = vfprintf( stderr, fmt, args );
	if( r >= 0 && fputc( '\n', stderr ) == '\n' ) {
		r++;
	}
	va_end( args );
	return r;
}
