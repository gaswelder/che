#import crypt/sha1

bool test() {
	char input[1000001] = {0};
	memset(input, 'a', 1000000);
	input[1000000] = '\0';

	sha1.sha1sum_t sum = sha1.sha1_str(input);

	char hex[41] = "";
	if (!sha1.sha1_hex(sum, hex, 41)) {
		return false;
	}

	return !strcmp("34aa973cd4c4daa4f61eeb2bdbad27316534016f", hex);
}

int main() {
	if (test()) {
		return 0;
	} else {
		return 1;
	}
}
