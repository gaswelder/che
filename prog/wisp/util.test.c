#import util.c
#import test

int main() {
	test.streq(eval.addslashes("Hello\" there!"), "\"Hello\\\" there!\"");
	return test.fails();
}
