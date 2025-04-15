#import eval.c
#import parsebuf
#import read.c

int main() {
	parsebuf.parsebuf_t *b = parsebuf.from_stdin();
	rt.item_t *input = read.read(b);
	rt.item_t *r = eval.eval(input);

	char buf[4096];
	rt.print(r, buf, 4096);
	puts(buf);
	return 0;
}
