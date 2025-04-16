#import eval.c
#import parsebuf
#import read.c
#import test
#import tok.c

typedef {
	const char *in, *out;
} case_t;

case_t cases[] = {
	// Echoes a number
	{"12", "12"},

	// Basic math
	{"(+ 137 349)", "486"},
	{"(- 1000 334)", "666"},
	{"(* 2 6)", "12"},
	{"(/ 10 5)", "2"},
	{"(+ 2.7 10)", "12.7"},

	// >2 args in math
	{"(+ 21 35 12 7)", "75"},
	{"(* 25 4 12)", "1200"},

	// Comparisons
	{"(eq? 1 1)", "true"},
	{"(eq? 1 2)", "NULL"},

	// List ops
	{"(cons 1 nil)", "(1)"},
	{"(cons 1 (2))", "(1 2)"},

	// Reads multiple forms, returns the last evaluation
	{"1 2", "2"},

	// Constant defines
	{"(define x 1) x", "1"},

	// Function defines
	{"(define (sqr x) (* x x)) (sqr 11)", "121"},
	{
		"(define pi 3.14159)"
		"(define (circumference radius) (* 2 pi radius))"
		"(circumference 10)",
		"62.8318"
	},

	// ...
	{"(apply cons (quote (a (b c))))", "(a b c)"},
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
