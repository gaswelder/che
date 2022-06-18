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

The language differences are outlined below.

## No include headers

The C languange specifies a library that's well known and constant. There is no
point in writing these headers every time, so hello world program can be just
this:

```c
int main() {
	puts("Howdy, Globe!");
}
```

## No function prototypes

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

## No need in `struct`

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

## Declaration lists

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

## The `pub` keyword

In C, in order to make functions private to the module, they are declared as
`static`. But it's more typicaly to export things, not hide them, so `static` is
gone, and `pub` is introduced:

```c
// f1 will be accessible from importing moduled, but f2 will not and will be
// compiled static.
pub int f1() {...}
int f2() {...}
```

`main` doesn't have to be marked public, so “hello world” still is:

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

Function-local static variables are gone. Module-local variables are always
assumed static, a module can't export a variable, so there is no `pub` for
variables.

```c
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

## Private enums and typedefs

Enums and typedefs may be marked `pub` to become importable. By default all
`enum` and `typedef` declarations are private.

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

## The nelem macro

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
