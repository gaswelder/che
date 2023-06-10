#import crypt/md5

typedef {
	const char *d;
	const char *s;
} kv_t;

kv_t tests[] = {
	{"d41d8cd98f00b204e9800998ecf8427e", ""},
	{"0cc175b9c0f1b6a831c399e269772661", "a"},
	{"900150983cd24fb0d6963f7d28e17f72", "abc"},
	{"f96b697d7cb7938d525a2f31aaf161d0", "message digest"},
	{"c3fcd3d76192e4007dfb496cca67e13b", "abcdefghijklmnopqrstuvwxyz"},
	{"d174ab98d277d9f5a5611c2c9f419d9f",
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"},
	{"57edf4a22be3c955ac49da2e2107b67a", "12345678901234567890"
		"123456789012345678901234567890123456789012345678901234567890"}
};

bool rfctest() {
	md5sum_t s = {0};
	md5str_t buf = "";

	for (size_t i = 0; i < nelem(tests); i++) {
		md5.md5_str(tests[i].s, s);
		md5.md5_sprint(s, buf);
		if (!strcmp(buf, tests[i].d) == 0) {
			printf("* FAIL (%s != %s)\n", buf, tests[i].d);
			return false;
		}
	}
	return true;
}

int main() {
	if (rfctest()) {
		return 0;
	}
	return 1;
}
