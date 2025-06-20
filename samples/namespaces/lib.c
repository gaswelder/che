#import lib2.c

pub enum {
    ONE = 1,
    TWO = 2
}

pub typedef {
	int a;
	lib2.foo_t b;
} foo_t;

pub void f() {
    puts("OK");
}
