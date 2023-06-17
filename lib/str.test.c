#import str
#import test

int main() {
	str.str *sb = str.str_new();

	char *s = "0123456789";
	char *p = s;
	while (*p) {
		test.truth("addc", str.str_addc(sb, *p));
		p++;
	}

	test.truth("length check", str.str_len(sb) == strlen(s));
	test.streq(str.str_raw(sb), s);
	str.str_free(sb);
	return test.fails();
}
