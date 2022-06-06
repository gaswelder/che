#import mem
#import panic

/*
 * A test program that copies a file into a "mem" buffer
 * and then prints it out from it.
 */

int main(int argc, char **argv)
{
	if(argc != 2) {
		panic("Usage: output <filename>");
	}

	FILE *f = fopen(argv[1], "rb");
	if(!f) {
		panic("fopen failed");
	}

	mem_t *m = memopen();
	if(!m) {
		panic("memopen failed");
	}

	/*
	 * Read the file into the buffer
	 */
	char buf[8] = "";
	while(1) {
		size_t len = fread(buf, 1, 8, f);
		if(!len) {
			break;
		}
		size_t r = memwrite(m, buf, len);
		if(r < len) {
			panic("memwrite failed");
			break;
		}
	}
	fclose(f);

	/*
	 * Rewind the buffer and output it.
	 */
	memrewind(m);
	while(1) {
		int c = memgetc(m);
		if(c == EOF) break;
		printf("%c", c);
	}
	memclose(m);
	return 0;
}
