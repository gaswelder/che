import "crypt/md5"

struct kv {
	const char *d;
	const char *s;
};

typedef struct kv kv_t;

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

int main()
{
	md5sum s;
	md5str buf;

	size_t n = sizeof(tests)/sizeof(tests[0]);
	for(size_t i = 0; i < n; i++) {
		md5_str(tests[i].s, s);
		md5_sprint(s, buf);

		printf("%s\n", tests[i].s);
		if(strcmp(buf, tests[i].d) == 0) {
			printf("* OK (%s)\n", buf);
		}
		else {
			printf("* FAIL (%s != %s)\n", buf, tests[i].d);
		}
		putchar('\n');
	}
}
