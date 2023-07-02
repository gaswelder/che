#include <sys/time.h>

typedef struct tm tm_t;
pub typedef struct timeval timeval_t;

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

/*
 * Returns current system time as an opaque object.
 */
pub timeval_t get_time() {
    timeval_t time;
    OS.gettimeofday(&time,0);
    return time;
}

/*
 * Returns unix timestamp in microseconds for the given time object.
 */
pub int32_t time_usec(timeval_t *t) {
    return t->tv_sec * 1000000L + t->tv_usec;
}
