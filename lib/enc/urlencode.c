#import writer

const char HEX[] = "0123456789ABCDEF";

// Writes URL-encoded representation of data into the writer.
pub int write(writer.t *w, const char *data, size_t datalen) {
	int len = 0;
	uint8_t tmp[3] = {};
	for (size_t i = 0; i < datalen; i++) {
		uint8_t c = (uint8_t) data[i];
		if (isalpha(c) || isdigit(c)) {
			if (writer.write(w, &c, 1) != 1) {
				return -1;
			}
			len++;
			continue;
		}
		tmp[0] = '%';
		tmp[1] = HEX[(c >> 4) & 0x0F];
		tmp[2] = HEX[c & 0x0F];
		if (writer.write(w, tmp, 3) != 3) {
			return -1;
		}
		len += 3;
	}
	return len;
}
