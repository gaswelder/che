#import eval.c
#import parsebuf
#import read.c
#import rt.c
#import test

typedef {
	const char *in, *out;
} case_t;

case_t cases[] = {
	{"12", "12"},
	{"(apply cons (quote (a (b c))))", "(a b c)"}
};

int main() {
	char buf[4096];
	for (size_t i = 0; i < nelem(cases); i++) {
		case_t c = cases[i];
		parsebuf.parsebuf_t *b = parsebuf.from_str(c.in);
		rt.item_t *x = read.read(b);
		x = eval.eval(x);
		rt.print(x, buf, 4096);
		test.streq(buf, c.out);
		parsebuf.buf_free(b);
	}
	return test.fails();
}
