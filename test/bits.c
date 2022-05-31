#import bitreader
#import cli

/*
 * Reads and prints a file using the bitreader
 * to read characters.
 */
int main(int argc, char *argv[])
{
	if(argc != 2) {
		fatal("Usage: bits <textfile>");
	}

	FILE *f = fopen(argv[1], "rb");
	bits_t *b = bits_new(f);

	while(1) {
		int c = bits_getn(b, 8);
		if(c < 0) {
			break;
		}
		printf("%c", c);
	}

	bits_free(b);
	fclose(f);
}
