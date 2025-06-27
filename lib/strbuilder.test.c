#import strbuilder
#import test

int main() {
	strbuilder.str *sb = strbuilder.str_new();

	char *s = "0123456789";
	char *p = s;
	while (*p != '\0') {
		test.truth("addc", strbuilder.str_addc(sb, *p));
		p++;
	}

	test.truth("length check", strbuilder.str_len(sb) == strlen(s));
	test.streq(strbuilder.str_raw(sb), s);
	strbuilder.str_free(sb);
	return test.fails();
}
