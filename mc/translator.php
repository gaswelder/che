<?php

class mc_trans
{
	/*
	 * Translates MC code into C code
	 */
	static function translate( $code )
	{
		$prototypes = array();
		$types = array();
		$body = array();
		$headers = array();

		$n = count($code);
		for($i = 0; $i < $n; $i++)
		{
			$element = $code[$i];

			if( $element instanceof c_typedef ) {
				self::type( $element->type, $headers );
				$types[] = $element;
				continue;
			}

			if($element instanceof c_import) {
				$imp = get_import($element->path, $element->dir);
				array_splice($code, $i, 1, $imp->code);
				$i--;
				$n = count($code);
				continue;
			}

			if( $element instanceof c_structdef ) {
				$types[] = new c_structdef($element->name);
				foreach( $element->fields as $list ) {
					self::varlist($list, $headers );
				}
			}

			if( $element instanceof c_func ) {
				self::proto($element->proto, $headers);
				self::body( $element->body, $headers );
				$prototypes[] = $element->proto;
			}

			if($element instanceof c_prototype) {
				self::proto($element, $headers);
			}

			/*
			 * Mark module-global variables as static.
			 */
			if($element instanceof c_varlist) {
				$element->stat = true;
			}

			$body[] = $element;
		}

		$out = array();

		$headers = array_keys( $headers );
		sort( $headers );
		foreach( $headers as $h ) {
			$out[] = new c_macro( 'include', "<$h.h>" );
		}

		$out = array_merge( $out, $types, $prototypes, $body );
		return $out;
	}

	private static function proto(c_prototype $p, &$headers)
	{
		$args = $p->args;
		foreach($args->lists as $list) {
			self::varlist($list, $headers);
		}
		self::type($p->type, $headers);
	}

	private static function body( $body, &$headers )
	{
		foreach( $body->parts as $part )
		{
			$cn = get_class( $part );
			switch( $cn )
			{
				case 'c_varlist':
					self::varlist($part, $headers);
					break;

				case 'c_if':
					self::expr( $part->cond, $headers );
					self::body( $part->body, $headers );
					if( $part->else ) {
						self::body( $part->else, $headers );
					}
					break;

				case 'c_while':
					self::expr( $part->cond, $headers );
					self::body( $part->body, $headers );
					break;

				case 'c_for':
					if($part->init instanceof c_varlist) {
						self::varlist($part->init, $headers);
					}
					else {
						self::expr( $part->init, $headers );
					}
					self::expr( $part->cond, $headers );
					self::expr( $part->act, $headers );
					self::body( $part->body, $headers );
					break;

				case 'c_switch':
					self::expr( $part->cond, $headers );
					foreach( $part->cases as $case ) {
						self::body( $case[1], $headers );
					}
					break;

				case 'c_return':
				case 'c_expr':
					self::expr( $part, $headers );
					break;

				default:
					var_dump( "1706", $part );
					exit(1);
			}
		}
	}

	private static function varlist(c_varlist $l, &$headers)
	{
		self::type($l->type, $headers);
		foreach($l->values as $e) {
			self::expr($e, $headers);
		}
	}

	private static function type(c_type $type, &$headers )
	{
		foreach( $type->l as $cast ) {
			if( !is_string( $cast ) ) continue;
			self::id( $cast, $headers );
		}
	}

	private static function id( $id, &$headers )
	{
		$h = self::get_header( $id );
		if( $h ) $headers[$h] = true;
	}

	private static function expr( $e, &$headers )
	{
		if( !$e ) return;

		foreach( $e->parts as $part )
		{
			if( !is_array( $part ) ) continue;

			foreach( $part as $op ) {
				switch( $op[0] )
				{
					case 'id':
						self::id( $op[1], $headers );
						break;

					case 'call':
						foreach( $op[1] as $expr ) {
							self::expr( $expr, $headers );
						}
						break;

					case 'expr':
						self::expr( $op[1], $headers );
						break;

					case 'cast':
						self::type( $op[1]->type, $headers );
						break;

					case 'literal':
					case 'op':
					case 'sizeof':
					case 'index':
						break;

					default:
						var_dump( $op );
						exit;
				}
			}
		}
	}

	private static function get_header( $id )
	{
		if( !self::$init ) {
			self::$init = true;
			self::init();
		}
		foreach( self::$lib as $header => $ids ) {
			if( in_array( $id, $ids ) ) {
				return $header;
			}
		}
		return null;
	}

	private static function init()
	{
		foreach( self::$lib as $k => $str ) {
			$ids = array_filter( preg_split( '/\s+/', $str ) );
			self::$lib[$k] = $ids;
		}
	}

	private static $init = false;

	private static $lib = array(
		'arpa/inet' => '
			inet_ntop inet_ntoa
		',

		'netinet/in' => '
			sockaddr_in
		',

		'assert' => '
			assert',

		'ctype' => '
			isalnum isalpha isblank iscntrl isdigit isgraph
			islower isprint ispunct isspace isupper isxdigit tolower
			toupper',

		'errno' => '
			errno
		',

		'limits' => '
			INT_MAX INT_MIN UINT_MAX
		',

		'stdarg' => '
			va_list va_arg va_end va_start va_copy',

		'stdbool' => '
			true false bool',

		'stddef' => '
			ptrdiff_t size_t wchar_t NULL',

		'stdint' => '
			int8_t int16_t int32_t int64_t
			uint8_t uint16_t uint32_t uint64_t
			INT8_MIN INT16_MIN INT32_MIN INT64_MIN
			INT8_MAX INT16_MAX INT32_MAX INT64_MAX
			UINT8_MIN UINT16_MIN UINT32_MIN UINT64_MIN
			UINT8_MAX UINT16_MAX UINT32_MAX UINT64_MAX
			SIZE_MAX',

		'stdio' => '
			size_t FILE EOF
			remove rename tmpfile tmpnam fclose fflush fopen freopen
			setbuf setvbuf
			fprintf fscanf printf scanf snprintf sprintf sscanf
			vfprintf vfscanf vsnprintf vsprintf vsscanf fgetc fgets
			fputc fputs getc getchar gets putc putchar puts ungetc
			fread fwrite fgetpos fseek fsetpos ftell rewind clearerr
			feof ferror perror
		',
		'stdlib' => '
			RAND_MAX NULL EXIT_FAILURE EXIT_SUCCESS
			atof atoi atol atoll strtod stotof strtold strtol
			strtoll strtoul strtoull
			rand srand calloc free malloc realloc
			abort atexit exit getenv system
			bsearch qsort abs labs llabs
		',

		'string' => '
			memcpy memmove strcpy strncpy strcat strncat
			memcmp strcmp strncmp memset strerror strlen
		',

		'time' => '
			CLOCKS_PER_SEC clock_t time_t clock difftime mktime
			time asctime ctime gmtime localtime strftime
		',

		'unistd' => '
			chdir close getcwd getopt sleep optarg opterr
			optind optopt
		',

		'sys/select' => '
			fd_set
		',

		'sys/socket' => '
			connect SOCK_STREAM SOL_SOCKET socklen_t
		',

		'sys/types' => '
			ssize_t
		',

		'sys/stat' => '
			stat
		',

		'netdb' => '
			getaddrinfo
		'
	);
}

?>
