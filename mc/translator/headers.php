<?php
class tr_headers
{
	// Returns a list of standard C headers needed for the given code.
	static function add_headers(module $mod)
	{
		if (!self::$init) {
			self::$init = true;
			self::init();
		}

		$names = [];
		foreach ($mod->code as $element) {
			foreach ($element->typenames() as $name) {
				if ($name instanceof che_identifier) {
					$name = $name->format();
				}
				$names[] = $name;
			}
		}
		$names = array_unique($names);

		$headers = [];
		foreach ($names as $name) {
			$headers[] = self::get_header($name);
		}
		$headers = array_unique(array_filter($headers));
		sort($headers);
		return $headers;
	}

	private static function get_header($id)
	{
		foreach (self::$lib as $header => $ids) {
			if (in_array($id, $ids)) {
				return $header;
			}
		}
		return null;
	}

	private static function init()
	{
		foreach (self::$lib as $k => $str) {
			$ids = array_filter(preg_split('/\s+/', $str));
			self::$lib[$k] = $ids;
		}
	}

	private static $init = false;

	private static $lib = array(
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
			INT_MAX INT_MIN UINT_MAX LONG_MAX ULONG_MAX
		',

		'math' => '
			acos asin atan atan2 cos sin sinf tan
			acosh asinh atanh cosh sinh tanh
			exp exp2 expm1 frexp ilogb ldexp
			log10 log1p log2 logb modf scalbn scalbln
			fabs pow sqrt sqrtf
			ceil floor
			round roundf roundl
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
		'
	);
}
