#import eval.c
#import parsebuf
#import read.c
#import test
#import tok.c

typedef {
	const char *in, *out;
} case_t;

case_t cases[] = {
	{"12", "12"},
	{"1 2", "2"},
	{"(eq? 1 1)", "true"},
	{"(eq? 1 2)", "NULL"},
	{"(cons 1 nil)", "(1)"},
	{"(cons 1 (2))", "(1 2)"},
	{"(apply cons (quote (a (b c))))", "(a b c)"},
	{"(define x 1) x", "1"},
};

int main() {
	char buf[4096];
	for (size_t i = 0; i < nelem(cases); i++) {
		case_t c = cases[i];
		parsebuf.parsebuf_t *b = parsebuf.from_str(c.in);
		tok.tok_t **all = read.readall(b);
		tok.tok_t *x = eval.evalall(all);
		tok.print(x, buf, 4096);
		test.streq(buf, c.out);
		parsebuf.buf_free(b);
	}
	return test.fails();
}
