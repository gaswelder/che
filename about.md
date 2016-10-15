# MC - modified C

This program translates a slightly modified variant of C to the
standard C and compiles the result using the c99 compiler installed in
the system. This is an attempt to see what C might be if some manual
hassle was removed: copying function prototypes, meddling with headers
and composing the command line.


## No include headers

The C languange specifies a library that everyone knows. The
declarations are distributed along a set of headers. Since the library
is standard, there's no point in writing these headers manually, so a
hello world program can be just this:

	int main() {
		puts("Howdy, Globe!");
	}

The translator will put `#include <stdio.h>` in before passing
the result to the compiler.


## No function prototypes

There are two kinds of programmers: those who think "top down" and
those who think "bottom up". If someone happens to favour the top-down
approach, they will have to write function prototypes at the top of the
file because C compilers work bottom-up. The translator will generate
the prototypes automatically, so this example will work:

	int main() {
		func2();
	}

	static void func2() {
		puts("Howdy, Globe!");
	}


## No forward declarations

There is no need to add forward declarations to make the following
example compile:

	struct a {
		struct b foo;
		struct a *next;
	};

	struct b {
		int foo;
	};


## Modules

Some time ago a single C source file used to be called a "module". It
is true in that sense that such a file can be compiled independently,
and functions and variables declared 'static' can be used only inside
that module. But to connect modules programmers have to do two things
manually: create and include "header files" (which is really an
automated copy-paste mechanism for prototypes and typedefs) and put the
modules in the compiler's command line. The MC translator does that
automatically:

	// main.c:
	import "module2"

	int main() {
		foo();
	}


	// module2.c:
	void foo() {
		puts("Howdy, Globe!");
	}

	// command line:
	$ mc main.c

The translator will replace every import statement with type
declarations and function prototypes extracted from the referenced
module. This is exactly what C programmers do, except they put the
declarations into a separate file and include it. The translator will
also track all the referenced files and put them to the C compiler's
command line.

Modules are searched first in the internal modules library, and then in
current working directory. If import name begins with "./", the module
is searched relatively to the directory of the importing module.

A module can be put into its own directory. The directory name is then
the module's name, and file "main" will be imported from it. For
example, if "foo" is a directory, then `import "foo"` will be
equivalent to `import "foo/main"`. The "main" source can use the "./"
rule to import its neighbours in it. This allows creating packages that
can be treated as single modules.


## Declaration lists

In C it is possible to declare multiple variables with a common type
like this:

	int a, b, c;

In MC is is also possible to do that with struct members and function
declarations:

	struct vec {
		int x, y, x;
	};

	struct vec sum(struct vec a, b) {
		...
	}

The original C rules still apply: pointer and array notations "stick"
to the identifiers, no the type, so in the following example the
structure `foo` has `char` member 'a' and `char *` member b:

	struct foo {
		char a, *b;
	};


## No preprocessor

There is no preprocessor, so almost all macros are gone. `#define`
statements are processed by the same parser as the normal language and
are used only to define constants.


## The `pub` keyword

In C, in order to make functions private to the module, they have to be
declared as `static`. But more often than not it is more convenient to
assume they all are private and explicitly mark the "public" ones. Thus
`static` is gone, and `pub` is introduced:

	// f1 will get imported, but f2 will not.
	pub int f1() {...}
	int f2() {...}

There is an exception for `main`: it's always assumed public, so "hello
world" still is:

	int main() {
		puts("Howdy, Globe!");
	}

In C variables can be `static` two. There are two kinds. One is
function-local, the other is module-local:

	// *** This is C, mind you ***
	// module-local variable
	static long seed;

	int count() {
		// function-local variable
		static int n;
		n++;
		return n;
	}

	long foo() {
		return seed++ * count();
	}

Function-local variables are usually caches of some kind, and they are
the same as the module-local ones, only can't be seen by other
functions. We always can move `n` out, possibly renaming it to
something like `current_count`, and deal only with module-local
variables. And those, just like functions, can be assumed private to
the module, so the result will be:

	// Note that these variables will still be initialized to zero
	// just like in C.
	long seed;
	int current_count;

	int count() {
		current_count++;
		return current_count;
	}

	pub long foo() {
		return seed++ * count();
	}

There is no `pub` for variables.
