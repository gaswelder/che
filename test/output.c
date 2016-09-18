import "memio"
import "cli"

/*
 * A test program that copies a file into a MEM buffer
 * and then prints it out from it.
 */

int main(int argc, char **argv)
{
	if(argc != 2) {
		fatal("Usage: output <filename>");
	}

	FILE *f = fopen(argv[1], "rb");
	if(!f) {
		fatal("fopen failed");
	}

	MEM *m = memopen("foo", "bar");
	if(!m) {
		fatal("memopen failed");
	}

	/*
	 * Read the file into the buffer
	 */
	char buf[8];
	while(1) {
		size_t len = fread(buf, 1, 8, f);
		if(!len) {
			break;
		}
		size_t r = memwrite(buf, 1, len, m);
		if(r < len) {
			err("memwrite failed");
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
