#include <sys/time.h>

typedef struct tm tm_t;
pub typedef struct timeval timeval_t;
pub typedef {
    timeval_t timeval;
} t;


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
pub t now() {
    t time = {};
    // timeval, timezone. timezone is obsolete, pass NULL.
    OS.gettimeofday(&time.timeval, NULL);
    return time;
}

/*
 * Returns unix timestamp in microseconds for the given time object.
 */
pub int usec(t *val) {
    return val->timeval.tv_sec * 1000000L + val->timeval.tv_usec;
}

pub int ms(t *val) {
    return usec(val) / 1000;
}

pub t add(t time, int addition, int unit) {
    if (unit == SECONDS) {
        time.timeval.tv_sec += addition;
    } else {
        time.timeval.tv_usec += addition * unit;
    }
    return time;
}

/*
 * Returns true if time b is greater than time a.
 */
pub bool isinc(t a, b) {
    if (b.timeval.tv_sec > a.timeval.tv_sec) {
        return true;
    }
    if (b.timeval.tv_sec < a.timeval.tv_sec) {
        return false;
    }
    return b.timeval.tv_usec > a.timeval.tv_usec;
}

pub enum {
    SECONDS = 1000000 // microseconds
};

pub typedef {
    int usec;
} duration_t;

pub duration_t duration(int val, int unit) {
    duration_t r = {};
    r.usec = val * unit;
    return r;
}

pub int durationval(duration_t val, int unit) {
    return val.usec / unit;
}
