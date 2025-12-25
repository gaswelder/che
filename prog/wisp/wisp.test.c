#import eval.c
#import test

int main() {
	eval.wisp_init();

	test.truth("symbol interning", eval.symbol("symbol") == eval.symbol("symbol"));
	test.streq(*eval.SYMNAME(eval.symbol("str")), "str");

	/* dynamic scoping */
	eval.val_t *so = eval.symbol("s");
	eval.val_t *a = eval.symbol("a");
	eval.val_t *b = eval.symbol("b");
	eval.val_t *c = eval.symbol("c");
	eval.SET (so, a);
	eval.sympush (so, b);
	eval.sympush (so, c);

	test.truth("symbol push/pop 1", eval.GET (so) == c);
	eval.sympop (so);
	test.truth("symbol push/pop 2", eval.GET (so) == b);
	eval.sympop (so);
	test.truth("symbol push/pop 3", eval.GET (so) == a);

	return test.fails();
}
