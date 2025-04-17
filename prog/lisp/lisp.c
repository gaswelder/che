#import eval.c
#import parsebuf
#import read.c
#import tok.c

int main() {
	char buf[4096];

	parsebuf.parsebuf_t *b = parsebuf.stdin();
	while (true) {
		tok.tok_t *in = read.read(b);
		if (!in) break;
		printf("> ");
		tok.print(in, buf, 4096);
		puts(buf);
		tok.tok_t *out = eval.eval(in);
		tok.print(out, buf, 4096);
		puts(buf);
	}
	return 0;
}
