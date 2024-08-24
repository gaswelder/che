#import crypt/sha1
#import test

int main() {
	sha1.digest_t hash = {};

	for (int i = 0; i < 1000000; i++) sha1.add(&hash, 'a');
	sha1.end(&hash);

	char hex[41] = {};
	test.truth("format", sha1.format(&hash, hex, sizeof(hex)));
	test.streq("34aa973cd4c4daa4f61eeb2bdbad27316534016f", hex);
	return test.fails();
}
