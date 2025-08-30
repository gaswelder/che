#include <sys/time.h>
#include <time.h>

typedef struct tm tm_t;
typedef struct timespec timespec_t;

pub enum {
    US = 1, // base unit
    MS = 1000,
    SECONDS = 1000000,
	MINUTES = 60000000,
}

pub typedef struct timeval timeval_t;
pub typedef {
    timeval_t timeval;
} t;

pub typedef {
	int64_t us;
} duration_t;

// Returns the sum of durations a and b.
pub duration_t dur_add(duration_t a, b) {
	duration_t r = { .us = a.us + b.us };
	return r;
}

pub void dur_set(duration_t *d, int64_t val, int unit) {
	d->us = val * unit;
}

pub int64_t dur_us(duration_t *d) {
	return d->us;
}

pub duration_t newdur(int64_t val, int unit) {
	duration_t d = { .us = unit * val };
	return d;
}

// Splits absolute duration in microseconds into components.
void split(int64_t val, int *parts) {
	parts[0] = val % 1000; // us
	val /= 1000; // val is sum_ms now

	parts[1] = val % 1000; // ms
	val /= 1000; // val is sum_s now

	parts[2] = val % 60; // s
	val /= 60; // val is sum_min now

	parts[3] = val % 60; // min
	val /= 60; // val is sum_h now

	parts[4] = val; // sum_h
}

// Formats duration d into the given buffer.
// Returns false if the buffer is too small.
pub bool dur_fmt(duration_t *d, char *buf, size_t bufsize, const char *fmt) {
	int parts[5];
	split(d->us, parts);
	int len;

	switch str (fmt) {
		case "logfile": {
			int ms = parts[1] + 1000 * parts[2];
			double fsec = (double) ms / 1000.0;
			int mm = parts[3] + 60 * parts[4];
			if (mm > 0) {
				len = snprintf(buf, bufsize, "%dm %f s", mm, fsec);
			} else {
				len = snprintf(buf, bufsize, "%f s", fsec);
			}
		}
		case "mm:ss": {
			int ss = parts[2];
			int mm = parts[3] + 60 * parts[4];
			len = snprintf(buf, bufsize, "%02d:%02d", mm, ss);
		}
		case "mm:ss.ms": {
			int ms = parts[1];
			int ss = parts[2];
			int mm = parts[3] + 60 * parts[4];
			len = snprintf(buf, bufsize, "%02d:%02d.%03d", mm, ss, ms);
		}
		default: {
			panic("unknown format: %s", fmt);
		}
	}
	return (size_t) len + 1 <= bufsize;
}

pub int parse_duration(const char *s, duration_t *d) {
	int parts[3] = {};
    int psize = 0;

    const char *p = s;
    p = readint(p, &parts[psize++]);
    while (*p == ':') {
        p++;
        p = readint(p, &parts[psize++]);
    }

    switch (psize) {
        case 2: {
			d->us = (parts[0] * 60 + parts[1]) * SECONDS;
            return 0;
        }
        default: {
            panic("unimplemented format: %s", s);
        }
    }
}

const char *readint(const char *p, int *r) {
    int n = 0;
    while (isdigit(*p)) {
        n *= 10;
        n += (int) *p - (int) '0';
        p++;
    }
    *r = n;
    return p;
}


/**
 * Returns current system time as an opaque object.
 */
pub t now() {
    t time = {};
    // timeval, timezone. timezone is obsolete, pass NULL.
    OS.gettimeofday(&time.timeval, NULL);
    return time;
}

/**
 * Returns a time object for the given unix time.
 */
pub t from_unix(int64_t seconds) {
	t val = {};
	val.timeval.tv_sec = seconds;
	return val;
}

pub int64_t sub(t a, b) {
    return SECONDS * (a.timeval.tv_sec - b.timeval.tv_sec)
        + US * (a.timeval.tv_usec - b.timeval.tv_usec);
}

pub enum {
    FMT_FOO
}

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
	int64_t s = dt / SECONDS;
	int64_t us = dt % SECONDS;
    timespec_t t = {
        .tv_sec = s,
        .tv_nsec = us * 1000
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
