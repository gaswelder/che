#import bitreader

bool test() {
	FILE *f = tmpfile();
	fputc(128, f);
	fseek(f, 0, SEEK_SET);
	
	bitreader.bits_t *b = bitreader.bits_new(f);
	int r = 0;
	for (int i = 0; i < 8; i++) {
		r = r * 2 + bitreader.bits_getn(b, 1);
	}
	if (r != 128) {
		bitreader.bits_free(b);
		fclose(f);
		return false;
	}

	if (bitreader.bits_getn(b, 1) != EOF) {
		bitreader.bits_free(b);
		fclose(f);
		return false;
	}
	bitreader.bits_free(b);
	fclose(f);
	return true;
}

int main() {
	if (test()) {
		return 0;
	}
	return 1;
}
