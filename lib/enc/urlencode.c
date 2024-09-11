const char HEX[] = "0123456789ABCDEF";

/**
 * Puts url-encoded representation of buf into s.
 */
pub void enc(char *s, const char *buf, size_t bufsize) {
	char *p = s;
	for (size_t i = 0 ; i < bufsize; i++) {
		char c = buf[i];
		if (isalpha(c) || isdigit(c)) {
			*p++ = c;
			continue;
		}
		*p++ = '%';
		*p++ = HEX[(c >> 4) & 0x0F];
		*p++ = HEX[c & 0x0F];
	}
	*p++ = '\0';
}
