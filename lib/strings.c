// Allocates a formatted string.
// The caller must call free on the string when done.
pub char *newstr(const char *format, ...) {
	// Find out the length.
	va_list args = {};
	va_start(args, format);
	int len = vsnprintf(NULL, 0, format, args);
	va_end(args);
	if (len < 0) {
		panic("failed to format the string");
	}

	// Actually allocate and format.
	char *s = calloc!(len + 1, 1);
	va_start(args, format);
	vsnprintf(s, len + 1, format, args);
	va_end(args);

	return s;
}

pub char *newsubstr(const char *s, int p1, p2) {
	char *buf = calloc!(p2-p1 + 1, 1);
    int p = 0;
    for (int i = p1; i < p2; i++) {
        buf[p++] = s[i];
    }
    return buf;
}

// Trims spaces at both sides of s.
pub void trim(char *s) {
	char *left = s;
	while (*left != '\0' && isspace(*left)) {
		left++;
	}
	if (left > s) {
		memmove(s, left, strlen(left));
	}
	rtrim(s, " \t\r\n");
}

// Trims all characters from the set at the end of string s by inserting zeros.
pub void rtrim(char *s, *set) {
	size_t n = strlen(s);
	while (n > 0 && strchr(set, s[n-1])) {
		s[n-1] = '\0';
		n--;
	}
}

/*
 * Splits string `str` using `separator` and puts allocated substrings into
 * `result`. Stops when the whole string is processed or when the number of
 * substrings gets to `result_size`. Returns the number of substrings put into
 * `result`.
 * If `str` is null, returns 0.
*/
pub size_t split(const char *separator, const char *str, char **result, size_t result_size) {
	if (!str) {
		return 0;
	}
	size_t count = 0;
	const int sepsize = strlen(separator);
	const char *current = str;

	while (true) {
		// If there's only one slot left in the output list, put the whole
		// remaining string there and return.
		if (count == result_size - 1) {
			result[count++] = newstr("%s", current);
			break;
		}

		// Find next separator position.
		char *pos = strstr(current, separator);
		if (pos) {
			// Push string from current to pos and jump after the found separator.
			result[count++] = newsubstr(current, 0, pos - current);
			current = pos + sepsize;
			continue;
		} else {
			// Push the remaining string.
			result[count++] = newstr("%s", current);
			break;
		}
	}
	return count;
}

// Compares ASCII strings a and b ignoring case.
// Returns true if the strings are equal.
pub bool casecmp(const char *a, *b) {
	const char *x = a;
	const char *y = b;
	while (*x != '\0' && *y != '\0') {
		if (tolower(*x) != tolower(*y)) {
			return false;
		}
		x++;
		y++;
	}
	return *x == '\0' && *y == '\0';
}

// Returns true if string starts with prefix.
pub bool starts_with(const char *string, *prefix) {
	const char *x = string;
	const char *y = prefix;
	while (*x != '\0' && *y != '\0') {
		if (*x != *y) {
			return false;
		}
		x++;
		y++;
	}
	return *y == '\0';
}

pub bool starts_with_i(const char *string, *prefix) {
	const char *x = string;
    const char *y = prefix;
    while (*x != '\0' && *y != '\0') {
        if (tolower(*x) != tolower(*y)) {
            return false;
        }
		x++;
		y++;
    }
    return *y == '\0';
}

pub bool ends_with(const char *string, *suffix) {
	const char *x = string;
	const char *y = suffix;
	while (*x != '\0') x++;
	while (*y != '\0') y++;
	while (x > string && y > suffix) {
		if (*x != *y) {
			return false;
		}
		x--;
		y--;
	}
	return y == suffix;
}

pub bool eq(char *a, char *b) {
	return strcmp(a, b) == 0;
}

// Returns true if ASCII string s is all uppercase.
pub bool allupper(char *s) {
    for (char *c = s; *c != '\0'; c++) {
        if (islower(*c)) {
            return false;
        }
    }
    return true;
}

pub void fmt_bytes(size_t bytes, char *buf, size_t bufsize) {
	double k = 1024;
	double m = 1024 * k;
	double g = 1024 * m;
	double b = (double) bytes;
    if (b > g) {
        snprintf(buf, bufsize, "%.2f GB", b / g);
    } else if (b > 1024 * 1024) {
        snprintf(buf, bufsize, "%.2f MB", b / m);
    } else if (b > 1024) {
        snprintf(buf, bufsize, "%.2f KB", b / k);
    } else {
        snprintf(buf, bufsize, "%zu B", bytes);
    }
}

// Returns the character that corresponds to the ASCII
// digit n, where n is from [0, 9].
// Panics if n is not from the allowed range.
pub char ascii_digit(int n) {
	if (n < 0 || n > 9) {
		panic("invalid argument: %d", n);
	}
	return (int)'0' + n;
}

// Returns the number that corresponds to the ASCII
// digit character c: 0 for '0', 1 for '1' and so on.
// Returns -1 if c is not a digit character.
pub int num_from_ascii(int c) {
	if (!isdigit(c)) {
		return -1;
	}
	return c - (int)'0';
}
