const char HEX[] = "0123456789abcdef";

pub char *writebyte(uint8_t b, char *p) {
	*p++ = HEX[b/16];
	*p++ = HEX[b%16];
	return p;
}
