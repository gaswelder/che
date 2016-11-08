import "zio"
import "cli"

/*
 * A test program that copies a file into a "mem" buffer
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

	zio *m = zopen("mem", "", "");
	if(!m) {
		fatal("zopen failed");
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
		size_t r = zwrite(m, buf, len);
		if(r < len) {
			err("memwrite failed");
			break;
		}
	}
	fclose(f);

	/*
	 * Rewind the buffer and output it.
	 */
	zrewind(m);
	while(1) {
		int c = zgetc(m);
		if(c == EOF) break;
		printf("%c", c);
	}
	zclose(m);
	return 0;
}
