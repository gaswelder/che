#import eval.c
#import tokenizer
#import read.c
#import test
#import tok.c
#import opt

typedef {
	const char *in, *out;
} case_t;

case_t cases[] = {
	// Echoes a number
	{"12", "12"},

	// true is true
	{"true", "true"},

	// Basic math
	{"(+ 137 349)", "486"},
	{"(- 1000 334)", "666"},
	{"(* 2 6)", "12"},
	{"(/ 10 5)", "2"},
	{"(+ 2.7 10)", "12.7"},

	// Unary minus
	{"(- 1)", "-1"},

	// >2 args in math
	{"(+ 21 35 12 7)", "75"},
	{"(* 25 4 12)", "1200"},

	// Nested math
	{"(+ (* 3 (+ (* 2 4) (+ 3 5))) (+ (- 10 7) 6))", "57"},

	// Comparisons
	{"(eq? 1 1)", "true"},
	{"(eq? 1 2)", "NULL"},
	{"(> 1 2)", "NULL"},
	{"(< 1 2)", "true"},
	{"(= 1 1)", "true"},

	// Logicals
	{"(and (> 7 5) (< 7 10))", "true"},
	{"(and (> 7 5) (> 7 10))", "NULL"},
	{"(not 1)", "NULL"},
	{"(not (not 1))", "true"},

	// List ops
	{"(cons 1 nil)", "(1)"},
	{"(cons 1 (2))", "(1 2)"},

	// Reads multiple forms, returns the last evaluation
	{"1 2", "2"},

	// Constant defines
	{"(define x 2) x", "2"},
	{"(define x 2) (* x 5)", "10"},
	{"(define b (+ 1 2)) b", "3"},

	// Function defines
	{"(define (sqr x) (* x x)) (sqr 11)", "121"},
	{
		"(define (sqr x) (* x x))"
		"(sqr (+ 2 5))",
		"49"
	},
	{
		"(define pi 3.14159)"
		"(define (circumference radius) (* 2 pi radius))"
		"(circumference 10)",
		"62.8318"
	},

	// Conditionals
	{"(if (not 1) 1)", "NULL"},
	{
		"(define (abs x)"
			"(cond ((> x 0) x)"
			"((= x 0) 0)"
			"((< x 0) (- x)))) (abs (- 123))",
		"123"
	},
	{
		"(define (abs1 x) (if (< x 0) (- x) x)) (abs1 (- 123))",
		"123",
	},

	// Examples
	{"(apply cons (quote (a (b c))))", "(a b c)"},
	{
		"(define a 3)"
		"(define b (+ a 1))"
		"(+ a b (* a b))",
		"19"
	},
	{
		"(define (a-plus-abs-b a b) ((if (> b 0) + -) a b))"
		"(a-plus-abs-b 10 (- 2))",
		"12"
	}

};

tok.tok_t *evalstr(const char *s) {
	tokenizer.t *b = tokenizer.from_str(s);
	tok.tok_t **all = read.readall(b);
	tokenizer.free(b);

	return eval.evalall(all);
}

int main(int argc, char **argv) {
	bool last = false;
	opt.flag("l", "run only the last test", &last);
	opt.nargs(0, "");
	opt.parse(argc, argv);

	char buf[4096];
	for (size_t i = 0; i < nelem(cases); i++) {
		if (last && i != nelem(cases)-1) {
			continue;
		}
		case_t c = cases[i];

		tok.tok_t *x = evalstr(c.in);

		tok.print(x, buf, 4096);
		if (!test.streq(buf, c.out)) {
			puts(c.in);
		}
	}
	return test.fails();
}
