#import namespaceslib.c

int main() {
    // Can use an imported type in a variable declaration.
    namespaceslib.foo_t val = {};

	if (val.a != 0) panic("!");
	if (val.b.b != 0) panic("!");
	if (sizeof(val) != 8) panic("!");

    // Can use an imported constant.
	if (namespaceslib.ONE != 1) panic("!");

    // Can call an imported function.
    namespaceslib.f();

    // Can use in a switch.
    switch (val.a) {
        case namespaceslib.ONE: { panic("!"); }
        default: {}
    }

    f(10, val);

    // Can use ns as a variable name
    namespaceslib.foo_t x = {};
    (&x)->a++;
	if (sizeof(x) != 8) panic("!");
    return 0;
}

// Can use an imported type as an argument.
void f(const int y, namespaceslib.foo_t x, ...) {
	if (y != 10) panic("!");
	if (x.a != 0) panic("!");
}
