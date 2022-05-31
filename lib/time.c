
typedef struct tm tm_t;

/**
 * Puts current local time, formatted according to `fmt`, in the buffer `buf`.
 * Returns false on error.
 */
pub bool time_format(char *buf, size_t size, const char *fmt)
{
	time_t t = time(NULL);
    if (!t) {
        return false;
    }
	tm_t *ts = localtime(&t);
	return strftime(buf, size, fmt, ts);
}
