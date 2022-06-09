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

	char *right = left;
	while (*right) right++;
	right--;

	while (right > left && isspace(*right)) {
		*right = '\0';
		right--;
	}

	return left;
}
