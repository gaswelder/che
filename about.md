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
