import "zio"
import "cli"

int main(int argc, char **argv)
{
	if(argc != 2) {
		fatal("Usage: %s <filepath>", argv[0]);
	}

	zio *m = zopen("mem", "", "");
	zio *f = zopen("file", argv[1], "rb");

	char buf[16] = {};
	while(1) {
		int len = zread(f, buf, 16);
		if( len < 0 ) {
			fatal("read error");
		}
		if(len == 0) {
			break;
		}
		if( zwrite(m, buf, len) < len ) {
			fatal("write error");
		}
	}

	zclose(f);


	zrewind(m);
	while(1) {
		int len = zread(m, buf, 16);
		if(len < 0) {
			fatal("read error");
		}
		if(len == 0) {
			break;
		}
		fwrite(buf, 1, (size_t) len, stdout);
	}
	zclose(m);
}
