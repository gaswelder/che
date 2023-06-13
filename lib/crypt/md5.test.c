#import crypt/md5
#import test

md5.md5str_t buf = "";

const char *hash(const char *input) {
	md5.md5sum_t md = {0};
	md5.md5_str(input, md);
	md5.md5_sprint(md, buf);
	return buf;
}

int main() {
	test.streq("d41d8cd98f00b204e9800998ecf8427e", hash(""));
	test.streq("0cc175b9c0f1b6a831c399e269772661", hash("a"));
	test.streq("900150983cd24fb0d6963f7d28e17f72", hash("abc"));
	test.streq("f96b697d7cb7938d525a2f31aaf161d0", hash("message digest"));
	test.streq("c3fcd3d76192e4007dfb496cca67e13b", hash("abcdefghijklmnopqrstuvwxyz"));
	test.streq("d174ab98d277d9f5a5611c2c9f419d9f",
		hash("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"));
	test.streq("57edf4a22be3c955ac49da2e2107b67a",
		hash("12345678901234567890" "123456789012345678901234567890123456789012345678901234567890"));
	return test.fails();
}
