#import error
#include <sys/time.h>
#include <time.h>

/*
tm_t x = {
	.tm_sec = r.s, // 0-60 (60 for the leap second)
	.tm_min = r.m, // 0-59
	.tm_hour = r.h, // 0-23
	.tm_mday = r.D, // 1-31
	.tm_mon = r.M - 1, // 0-11
	.tm_year = r.Y - 1900, // years since 1900
	.tm_wday = -1, // 0-6
	.tm_yday = -1, // 0..365
	.tm_isdst = -1, // daylight saving; -1 = don't know
};
*/
typedef struct tm tm_t;
typedef struct timespec timespec_t;
typedef struct timeval timeval_t; // {tv_sec, tv_usec}

pub typedef {
	int Y, M, D, h, m, s, ms;
	int zh, zm; // zone hours and minutes
} iso_t;

pub enum {
    US = 1, // base unit
    MS = 1000,
    SECONDS = 1000000,
	MINUTES = 60000000,
}

// Represents a global instant of time.
pub typedef {
	int64_t seconds;
	int64_t useconds;
} t;

pub typedef {
	int64_t us;
} duration_t;

// Returns the sum of durations a and b.
pub duration_t dur_add(duration_t a, b) {
	duration_t r = { .us = a.us + b.us };
	return r;
}

int tzoffset() {
	tm_t *x = NULL;
	time_t n = time(NULL);

	x = OS.localtime(&n);
	int mins = x->tm_hour * 60 + x->tm_min;
	x = OS.gmtime(&n);
	mins -= (x->tm_hour * 60 + x->tm_min);

	// Possible diff range is -12:45 .. +14:45.
	int min = -(12*60 + 45);
	int max = 14*60+45;
	if (mins < min) {
		mins += 24 * 60;
	} else if (mins > max) {
		mins -= 24 * 60;
	}
	return mins;
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
		case "hh:mm:ss,ms": {
			len = snprintf(buf, bufsize, "%02d:%02d:%02d,%d", parts[4], parts[3], parts[2], parts[1]);
		}
		default: {
			panic("unknown format: %s", fmt);
		}
	}
	return (size_t) len + 1 <= bufsize;
}

// Parses duration from string s.
// Returns true on success,
// returns false and sets the error otherwise.
pub bool parse_duration(const char *s, duration_t *d, error.t *err) {
	int nums[3] = {};
    int numslen = 0;

    const char *p = s;
	if (*s == '\0') {
		error.set(err, "duration string is empty");
		return false;
	}

    p = readint(p, &nums[numslen++]);
	if (p == s) {
		error.set(err, "expected a number at %s", s);
		return false;
	}

	// up to two more of (":" <int>)
	if (*p == ':') {
		p++;
		p = readint(p, &nums[numslen++]);
	}
	if (*p == ':') {
		p++;
		p = readint(p, &nums[numslen++]);
	}

    switch (numslen) {
        case 2: {
			d->us = (nums[0] * 60 + nums[1]) * SECONDS;
            return true;
        }
    }
	error.set(err, "unknown format: %s", s);
	return false;
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

// Returns current time.
pub iso_t now() {
	timeval_t tv = {};
	OS.gettimeofday(&tv, NULL);
	iso_t r = fromunix(tv.tv_sec);
	r.ms = tv.tv_usec / 1000;
	return r;
}

// Returns time corresponding to the given unix time.
pub iso_t fromunix(int64_t seconds) {
	time_t s = seconds;
	tm_t *tm = OS.gmtime(&s);
	iso_t r = {
		.Y = tm->tm_year + 1900,
		.M = tm->tm_mon + 1,
		.D = tm->tm_mday,
		.h = tm->tm_hour,
		.m = tm->tm_min,
		.s = tm->tm_sec,
	};
	return r;
}

// Returns the time corresponding to the given ISO string.
pub iso_t parse_iso(const char *p) {
	iso_t r = {};

	// 2025-12-08T20:31:06+02:00
	// 2026-01-16T20:01:17.278Z
	const char *q = p;
	q = readint(q, &r.Y);
	if (*q++ != '-') panic("- expected");

	q = readint(q, &r.M);
	if (*q++ != '-') panic("- expected");

	q = readint(q, &r.D);
	if (*q++ != 'T') panic("T expected");

	q = readint(q, &r.h);
	if (*q++ != ':') panic(": expected");

	q = readint(q, &r.m);
	if (*q++ != ':') panic(": expected");

	q = readint(q, &r.s);
	if (*q == '.') {
		q++;
		q = readint(q, &r.ms);
	}

	// "Z" or "+02:00"
	if (*q == 'Z') {
		q++;
	} else if (*q == '+') {
		q++;
		q = readint(q, &r.zh);
		if (*q++ != ':') panic(": expected");
		q = readint(q, &r.zm);
	} else {
		panic("trailing input");
	}
	return r;
}

tm_t totm(iso_t r) {
	tm_t tm = {
		.tm_sec = r.s, // 0-60 (60 for the leap second)
		.tm_min = r.m, // 0-59
		.tm_hour = r.h, // 0-23
		.tm_mday = r.D, // 1-31
		.tm_mon = r.M - 1, // 0-11
		.tm_year = r.Y - 1900, // years since 1900
		.tm_wday = -1, // 0-6
		.tm_yday = -1, // 0..365
		.tm_isdst = -1, // daylight saving; -1 = don't know
	};
	return tm;
}

// Returns the difference a-b in milliseconds.
pub int64_t sub(iso_t a, b) {
	tm_t tm = totm(a);
	time_t au = OS.mktime(&tm);
	int64_t ms1 = ((int64_t) au) * 1000 + a.ms;

	tm = totm(b);
	time_t bu = OS.mktime(&tm);
	int64_t ms2 = ((int64_t) bu) * 1000 + b.ms;

	return ms1 - ms2;
}

pub bool fmt_iso_iso(iso_t val, char *buf, size_t bufsize) {
	snprintf(buf, bufsize, "%d-%02d-%02dT%02d:%02d:%02d.%03dZ", val.Y, val.M, val.D, val.h, val.m, val.s, val.ms);
	return true;
}

pub void iso_tolocal(iso_t *val) {
	if (val->zh != 0 || val->zm != 0) {
		panic("unhandled: non-utc iso");
	}
	val->m += tzoffset();
	while (val->m > 60) {
		val->m -= 60;
		val->h += 1;
	}
	// while (val->h > 23) {
	// 	val->D += 1;
	// 	val->h -= 23;
	// }
}


//
//
//

pub bool sleep(int64_t dt) {
	int64_t s = dt / SECONDS;
	int64_t us = dt % SECONDS;
    timespec_t t = {
        .tv_sec = s,
        .tv_nsec = us * 1000
    };
    return OS.nanosleep(&t, NULL) == 0;
}
