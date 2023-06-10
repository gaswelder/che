# Che – modified C language

This is a modified variant of C. The main goal is to get rid of the manual
hassle the standard C has: forward declarations, headers, build dependencies
and other annoyances.

It's a translator that converts a dialect into standard C99 and calls the
`c99` command installed in the system to produce the executable.
[`c99`](http://pubs.opengroup.org/onlinepubs/9699919799//utilities/c99.html)
is a command specified by POSIX, and GCC or Clang packages typically have this
binding installed. If not, you'd have to create a `c99` wrapper yourself for
whatever c99-compliant compiler you have.

Some libraries (for example, [net](lib/os/net.c)) depend on POSIX, so not
everything can be compiled on Windows or Mac. There was multi-platform support
initially, but that's not a goal just yet.

## Language differences

_No include headers_.
The C languange standard library is well defined, so there is no point in
writing its headers manually every time, so hello world program can be just
this, without any #includes:

```c
int main() {
	puts("Howdy, Globe!");
}
```

_No function prototypes_.
Forward declarations and prototypes are necessary for a single-pass C compiler.
This one is a multi-pass translator, so it can as well generate all needed
prototypes for the underlying C compiler. So this example will work:

```c
int main() {
	func2();
}

void func2() {
	puts("Howdy, Globe!");
}
```

_No need in `struct`_.
Instead of this:

```c
struct foo_s {
	int a;
	int b;
};
typedef struct foo_s foo_t;
```

we have this:

```c
typedef {
	int a;
	int b;
} foo_t;
```

_Declaration lists_.
In C it is possible to declare multiple variables with a common type like this:

```c
int a, b, c;
struct vec {
	int x, y, x;
};
```

Here it is also possible to do that with function parameters:

```c
int sum(int a, b, c) {
	...
}
```

_The `nelem` macro_.
One common idiom to get a static array's length is:

```c
for (int i = 0; i < sizeof(a) / sizeof(a[0]); i++) {
	...
}
```

It's often defined as macro in various places, so one of those is built in:

```c
for (int i = 0; i < nelem(a); i++) {
	...
}
```

## Modules

A single C source file used is called a "module". It's compiled independently
and linked with other compiled modules by the linker. Here this was taken
furtner to full-fledged import system:

    // main.c:
    ----------------
    #import module2

    int main() {
    	module2.foo();
    }
    ----------------

    // module2.c:
    ----------------
    pub void foo() {
    	puts("Howdy, Globe!");
    }
    ----------------

    // command line:
    $ che main.c
    ----------------

In C, in order to make functions private to the module, they are declared as
`static`. Here, `static` is gone, and `pub` is introduced:

```c
// f1 will be accessible from importing modules, but f2 will not and will be
// compiled as static.
pub int f1() {...}
int f2() {...}
```

As an exception, `main` doesn't have to be marked public, so “hello world” still is:

```c
int main() {
	puts("Howdy, Globe!");
}
```

Note also that instead of `foo` the importing module is calling `module2.foo`.
The translator will convert those namespaced calles into global names suitable
for a regular C compiler, taking care of possible name conflicts.

All the modules are automatically compiled and linked in the right order.
The build command is `che main.c`, as opposed to `c99 module1.c module2.c main.c`.

_No static variables_.
In C variables can be marked `static` in two cases. One is function-local,
the other is module-local. Both are essentially module globals:

```c
// *** C ***
// module-local variable
static long seed = 42;

int count() {
	// function-local variable
	static int n;
	n++;
	return n;
}

long foo() {
	return seed++ * count();
}
```

Here static variable are gone. Module variables are always assumed static,
and a module can't export variables, so there is no `pub` for them.

```c
// these variables can be accessed only inside this module.
long seed = 42;
int current_count = 0;

int count() {
	current_count++;
	return current_count;
}

pub long foo() {
	return seed++ * count();
}
```

Enums and typedefs may be marked `pub` to become importable.
By default all `enum` and `typedef` declarations are private.

## Testing

che has a built-in test runner.
Invoking `che test <dir>` will compile and run every `*.test.c` file in the given directory.
A `*.test.c` file is a regular program with `main`:

```c
// md5.test.c
#import crypt/md5

int main() {
	md5.md5sum_t s = {0};
	md5.md5str_t buf = "";

	char *expected = "0cc175b9c0f1b6a831c399e269772661";
	char *input = "a";
	md5.md5_str(input, s);
	md5.md5_sprint(s, buf);
	if (!strcmp(buf, want) == 0) {
		printf("* FAIL (%s != %s)\n", buf, want);
		return 1;
	}
	return 0;
}

```

If it returns a non-zero exit status, the runner will display the test as failed and will add the test's output.
