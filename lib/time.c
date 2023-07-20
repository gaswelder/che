#include <sys/time.h>
#include <time.h>

typedef struct tm tm_t;
typedef struct timespec timespec_t;

pub typedef struct timeval timeval_t;
pub typedef {
    timeval_t timeval;
} t;



pub enum {
    US = 1, // base unit
    MS = 1000,
    SECONDS = 1000000,
};

/**
 * Returns current system time as an opaque object.
 */
pub t now() {
    t time = {};
    // timeval, timezone. timezone is obsolete, pass NULL.
    OS.gettimeofday(&time.timeval, NULL);
    return time;
}

pub int64_t sub(t a, b) {
    return SECONDS * (a.timeval.tv_sec - b.timeval.tv_sec)
        + US * (a.timeval.tv_usec - b.timeval.tv_usec);
}

pub enum {
    FMT_FOO
};

pub const char *knownformat(int fmtid) {
    switch (fmtid) {
        case FMT_FOO: { return "%Y-%m-%d-%H-%M-%S"; }
        default: { return "(unknown time format)"; }
    }
}

pub bool format(t val, const char *fmt, char *buf, size_t n) {
    time_t x = val.timeval.tv_sec;
    tm_t *ts = localtime(&x);
    strftime(buf, n, fmt, ts);
    return true;
}


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

pub bool sleep(int64_t dt) {
    timespec_t t = {
        .tv_sec = dt / SECONDS,
        .tv_nsec = 0
    };
    return OS.nanosleep(&t, NULL) == 0;
}

/*
 * Returns unix timestamp in microseconds for the given time object.
 */
pub int usec(t *val) {
    return val->timeval.tv_sec * 1000000L + val->timeval.tv_usec;
}

pub int ms(t val) {
    return usec(&val) / 1000;
}

pub t add(t time, int addition, int unit) {
    if (unit == SECONDS) {
        time.timeval.tv_sec += addition;
    } else {
        time.timeval.tv_usec += addition * unit;
    }
    return time;
}
