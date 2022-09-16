#import bitreader

bool test() {
	FILE *f = tmpfile();
	fputc(128, f);
	fseek(f, 0, SEEK_SET);
	
	bits_t *b = bits_new(f);
	int r = 0;
	for (int i = 0; i < 8; i++) {
		r = r * 2 + bits_getn(b, 1);
	}
	if (r != 128) {
		bits_free(b);
		fclose(f);
		return false;
	}

	if (bits_getn(b, 1) != EOF) {
		bits_free(b);
		fclose(f);
		return false;
	}
	bits_free(b);
	fclose(f);
	return true;
}

int main() {
	if (test()) {
		return 0;
	}
	return 1;
}
