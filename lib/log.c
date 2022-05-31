#import cli

pub typedef struct tm tm_t;

pub void logmsg(const char *fmt, ...)
{
	char timestamp[64] = "";

	if (!date(timestamp, sizeof(timestamp), "%F %T")) {
		fatal("date failed");
	}

	fprintf(stderr, "%s ", timestamp);

	va_list args = {};
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fputc('\n', stderr);
}

int date(char *buf, size_t size, const char *fmt)
{
	time_t t = time(NULL);
	tm_t *ts = localtime(&t);
	return t && strftime(buf, size, fmt, ts);
}
