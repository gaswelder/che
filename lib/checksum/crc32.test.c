#import checksum/crc32
#import test

int main() {
	uint8_t *check = (uint8_t *) "123456789";

	test.truth("empty", crc32.update(0, check, 0) == 0);
	test.truth("123456789", crc32.update(0, check, 9) == 0xcbf43926);
	test.truth("seed", crc32.update(0x12345678, check, 0) == 0x12345678);

	// Splitting the input at every boundary must give the same result.
	uint32_t split = 0;
	for (size_t i = 0; i < 9; i++) {
		split = crc32.update(split, check + i, 1);
	}
	test.truth("split", split == 0xcbf43926);

	// Cross the slicing-by-8 and chunk boundaries, whole versus chunks.
	size_t n = 20000;
	uint8_t *data = calloc!(n, 1);
	for (size_t i = 0; i < n; i++) {
		data[i] = (uint8_t)(i * 7 + i / 251);
	}
	uint32_t whole = crc32.update(0, data, n);
	uint32_t parts = 0;
	size_t pos = 0;
	while (pos < n) {
		size_t take = 1000;
		if (take > n - pos) {
			take = n - pos;
		}
		parts = crc32.update(parts, data + pos, take);
		pos += take;
	}
	test.truth("chunked", whole == parts);

	free(data);

	return test.fails();
}
