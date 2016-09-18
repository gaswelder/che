import "zio"

int main(int argc, char **argv)
{
	if(argc != 2) {
		fprintf(stderr, "Usage: %s <filepath>\n", argv[0]);
		return 1;
	}

	zio *m = zopen("mem", "", "");
	zio *f = zopen("file", argv[1], "rb");

	char buf[16];
	while(1) {
		int len = zread(f, buf, 16);
		if( len < 0 ) {
			puts("read error");
			return 1;
		}
		if(len == 0) {
			break;
		}
		if( zwrite(m, buf, len) < len ) {
			puts("write error");
			return 1;
		}
	}

	zclose(f);


	zrewind(m);
	while(1) {
		int len = zread(m, buf, 16);
		if(len < 0) {
			puts("read error");
			return 1;
		}
		if(len == 0) {
			break;
		}
		fwrite(buf, 1, (size_t) len, stdout);
	}
	zclose(m);
}
