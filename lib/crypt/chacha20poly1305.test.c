#import crypt/chacha20poly1305
#import test

int main() {
	uint8_t key[32] = {};
	for (int i = 0; i < 32; i++) {
		key[i] = 128 + i;
	}
	uint8_t nonce[12] = {};
	uint8_t aad[12] = {};
	uint8_t tag[16] = {};
	uint8_t text[114] = {};
	unhex("070000004041424344454647", nonce);
	unhex("50515253c0c1c2c3c4c5c6c7", aad);
	const char *message = "Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, sunscreen would be it.";
	memcpy(text, message, sizeof(text));
	chacha20poly1305.seal(key, nonce, aad, 12, text, sizeof(text), tag);
	check("RFC 8439 AEAD ciphertext", text, "d31a8d34648e60db7b86afbc53ef7ec2a4aded51296e08fea9e2b5a736ee62d63dbea45e8ca9671282fafb69da92728b1a71de0a9e060b2905d6a5b67ecd3b3692ddbd7f2d778b8c9803aee328091b58fab324e4fad675945585808b4831d7bc3ff4def08e4b7a9de576d26586cec64b6116");
	check("RFC 8439 AEAD tag", tag, "1ae10b594f09e26a7e902ecbd0600691");
	uint8_t saved[114] = {};
	memcpy(saved, text, sizeof(text));
	tag[0] ^= 1;
	test.truth("AEAD rejects tampered tag", !chacha20poly1305.open(key, nonce, aad, 12, text, sizeof(text), tag));
	test.truth("no unauthenticated plaintext", !memcmp(saved, text, sizeof(text)));
	tag[0] ^= 1;
	test.truth("AEAD opens", chacha20poly1305.open(key, nonce, aad, 12, text, sizeof(text), tag));
	test.truth("AEAD plaintext", !memcmp(text, message, sizeof(text)));
	return test.fails();
}

void unhex(const char *s, uint8_t *out) {
	while (*s != '\0') {
		uint32_t v = 0;
		sscanf(s, "%2x", &v);
		*out++ = v;
		s += 2;
	}
}

void check(const char *name, const uint8_t *got, const char *hex) {
	uint8_t want[256] = {};
	unhex(hex, want);
	test.truth(name, memcmp(got, want, strlen(hex) / 2) == 0);
}
