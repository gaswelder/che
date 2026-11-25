#import error
#import scanner

pub typedef {
	char schema[10];
	char hostname[254];
	char port[10];
	char path[1024];
} t;

// Parses URL string s and returns the parsed struct.
// Sets err for invalid input.
pub t parse(const char *s, error.t *err) {
	t r = {};
	if (err->set) {
		return r;
	}
	scanner.t *buf = scanner.from_str(s);

	// http
	char *q = r.schema;
	while (scanner.more(buf) && scanner.peek(buf) != ':') {
		*q++ = scanner.get(buf);
	}

	// ://
	if (scanner.get(buf) != ':' || scanner.get(buf) != '/' || scanner.get(buf) != '/') {
		scanner.free(buf);
		error.set(err, "invalid URL: expected scheme://");
		return r;
	}

	// domain or ip address
	q = r.hostname;
	while (scanner.more(buf) && scanner.peek(buf) != ':' && scanner.peek(buf) != '/') {
		*q++ = scanner.get(buf);
	}

	q = r.port;
	if (scanner.peek(buf) == ':') {
		scanner.get(buf);
		while (scanner.more(buf) && scanner.peek(buf) != '/') {
			*q++ = scanner.get(buf);
		}
	}

	q = r.path;
	*q = '/';
	if (scanner.peek(buf) == '/') {
		while (scanner.more(buf)) {
			*q++ = scanner.get(buf);
		}
	}

	scanner.free(buf);
	return r;
}
