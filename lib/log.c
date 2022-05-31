#import time

pub bool logmsg(const char *fmt, ...)
{
	char timestamp[64] = "";

	if (!time_format(timestamp, sizeof(timestamp), "%F %T")) {
		return false;
	}

	fprintf(stderr, "%s ", timestamp);

	va_list args = {};
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fputc('\n', stderr);
	return true;
}
