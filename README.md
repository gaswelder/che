# Che – modified C language

This is a modified variant of C. The main goal is to get rid of the manual
hassle the standard C has: forward declarations, headers, build dependencies
and other annoyances.

It's a translator that converts the dialect into standard C99 and calls the
`c99` command installed in the system to produce the executable.
[`c99`](http://pubs.opengroup.org/onlinepubs/9699919799//utilities/c99.html)
is a command specified by POSIX, and GCC or Clang packages typically have this
binding installed. If not, you'd have to create a `c99` wrapper yourself for
whatever c99-compliant compiler you have.

Some libraries (for example, [net](lib/os/net.c)) depend on POSIX, so not
everything can be compiled on Windows. There was multi-platform support
initially, but I don't use Windows anymore.

## Language differences

_No include headers_.
The C languange specifies a library that's well known and constant. There is no
point in writing these headers every time, so hello world program can be just
this:

```c
int main() {
	puts("Howdy, Globe!");
}
```

_No function prototypes_.
Forward declarations and prototypes are necessary for a single-pass C compiler.
Here, it's not necessary, and the translator will make a pre-pass and generate
all prototypes automatically, so this example will work:

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

In Che it is also possible to do that also with function parameters:

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

Since it's often defined as macro, one of those is built in:

```c
for (int i = 0; i < nelem(a); i++) {
	...
}
```

## Modules

A single C source file used is called a "module". It's compiled independently
and linked with other compiled modules by the linker. Che makes this more
explicit with import statements:

    // main.c:
    ----------------
    #import module2

    int main() {
    	foo();
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

The translator will replace every import statement with type declarations and
function prototypes for the imported module. This is what you would do with
regular C through ".h" files.

All the modules are compiled and linked in a correct order automatically, the
build command is `che main.c`, as opposed to `c99 module1.c module2.c main.c`.

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

In C variables can be `static` too. There are two kinds of those. One is
function-local, the other is module-local:

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

Function-local static variables are gone.
Module-local variables are always assumed static, and a module can't export its variables, so there is no `pub` for variables.

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

## Module prefixes

```c
#import prng/mt
int main() {
	printf("%f\n", mt.randf());
	return 0;
}
```

is syntax sugar for

```c
#import prng/mt
int main() {
	printf("%f\n", mt_randf());
	return 0;
}
```

## Testing

che has a built-in test runner.
Invoking `che test <dir>` will compile and run every `*.test.c` file in the given directory.
A `*.test.c` file is a regular program with `main`:

```c
// md5.test.c
#import crypt/md5

int main() {
	md5sum_t s = {0};
	md5str_t buf = "";

	char *expected = "0cc175b9c0f1b6a831c399e269772661";
	char *input = "a";
	md5_str(input, s);
	md5_sprint(s, buf);
	if (!strcmp(buf, want) == 0) {
		printf("* FAIL (%s != %s)\n", buf, want);
		return 1;
	}
	return 0;
}

```

If it returns a non-zero exit status, the runner will display the test as failed and will add the test's output.
