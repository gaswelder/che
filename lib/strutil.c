/*
 * Creates a new formatted string in memory.
 */
pub char *newstr(const char *format, ... )
{
	va_list args = {};

	va_start(args, format);
	int len = vsnprintf(NULL, 0, format, args);
	va_end(args);

	if (len < 0) return NULL;
	
	char *buf = malloc(len + 1);
	if (!buf) return NULL;

	va_start(args, format);
	vsnprintf(buf, len + 1, format, args);
	va_end(args);

	return buf;
}

/**
 * Trims `s` by adding zeros at the end and adjusting the pointer at the
 * beginning. Modifies the buffer and returns the adjusted pointer. Don't
 * pass readonly buffers life those from static strings.
 */
pub char *trim(char *s) {
	char *left = s;
	while (*left && isspace(*left)) left++;

	rtrim(left, " \t\r\n");
	return left;
}

pub void rtrim(char *s, *set) {
	char *right = s;
	while (*right) right++;
	right--;

	while (right > s && strchr(set, *right)) {
		*right = '\0';
		right--;
	}
}

/*
 * Splits string `str` using `separator` and puts allocated substrings into
 * `result`. Stops when the whole string is processed or when the number of
 * substrings gets to `result_size`. Returns the number of substrings put into
 * `result`.
 * If `str` is null, returns 0.
*/
pub int strutil_split(char separator, char *str, char **result, size_t result_size) {
	if (!str) {
		return 0;
	}
	char *p = str;
	char *a = str;
	int count = 0;
	while ((size_t) count < result_size) {
		if (*p == separator || *p == '\0') {
			char *m = calloc(p - a + 1, 1);
			int i = 0;
			while (a != p) {
				m[i++] = *a;
				a++;
			}
			result[count++] = m;
			a = p+1;
		}
		if (*p == '\0') {
			break;
		}
		p++;
	}
	return count;
}
