# Che – modified C language

This is a modified variant of C. The main goal is to get rid of the
manual hassle the standard C has: forward declarations, meddling with
headers, composing the command line in the right order and other
annoyances like that.

This is a translator that converts its dialect to C99 and uses the
`c99` command installed in the system to produce the executable.
[`c99`](http://pubs.opengroup.org/onlinepubs/9699919799//utilities/c99.html)
is a command specified by POSIX, and typically GCC or Clang packages
have this binding installed. If not, it is easy to create manually for
any compiler that supports C99.

Currently the translator is drafted in PHP, and the syntax is still
changing. Some bundled libraries (for example, [net](lib/net.c)) depend
on POSIX, so not everything can be compiled on Windows.

The language differences are outlined below.


## No include headers

The C languange specifies a library that everyone knows. The
declarations are distributed along a set of headers. Since the library
is standard, there is no point in writing these headers manually, so a
hello world program can be just this:

```c
int main() {
	puts("Howdy, Globe!");
}
```

The translator will put `#include <stdio.h>` in before passing
the result to the C compiler.


## No function prototypes

There are two kinds of programmers: those who think “top down” and
those who think “bottom up”. If someone happens to favour the top-down
approach, they will have to write function prototypes at the top of the
file because C compilers work bottom-up. But Che will generate the
prototypes automatically, so this example will work:

```c
int main() {
	func2();
}

void func2() {
	puts("Howdy, Globe!");
}
```


## No forward declarations

There is no need to add forward declarations to make the following
example compile:

	```c
	struct a {
		struct b foo;
		struct a *next;
	};

	struct b {
		int foo;
	};
	```


## Declaration lists

In C it is possible to declare multiple variables with a common type
like this:

	```c
	int a, b, c;
	struct vec {
		int x, y, x;
	};
	```

In Che it is also possible to do that with function parameters:

	```c
	struct vec sum(struct vec a, b) {
		...
	}
	```

The original C rules still apply: pointer and array notations “stick”
to the identifiers, not the type.


## No preprocessor

There is no preprocessor, so all macros are gone. `#define` statements
are processed by the same parser as the normal language and are used
only to define constants.


## The `defer` statement

Borrowed from Go, this statement probably eliminates the last excuse
for using `goto` statements: arranging cleanup code. So instead of
this:

	```c
	bool foo() {
		FILE *f = fopen("foo", "rb");
		if(!f)
			return false;

		bool r = true;

		char *buf = malloc(42);
		if(!buf) {
			r = false;
			goto cleanup;
		}

		for(int i = 0; i < 42; i++) {
			if(!bar(buf[i])) {
				r = false;
				goto cleanup;
			}
		}

		cleanup:
			free(buf);
			fclose(f);

		return r;
	}
	```

we can write this:

	```c
	bool foo() {
		FILE *f = fopen("foo", "rb");
		if(!f) {
			return false;
		}
		defer fclose(f);

		char *buf = malloc(42);
		if(!buf) {
			return false;
		}
		defer free(buf);

		for(int i = 0; i < 42; i++) {
			if(!bar(buf[i])) {
				return false;
			}
		}

		return true;
	}
	```


## Modules and packages

Some time ago a single C source file used to be called a "module". It
is true in that sense that such a file can be compiled independently,
and functions and variables declared 'static' can be used only inside
that module. But to connect modules programmers have to do two things
manually: create and include “header files” (which is really a
semi-automated copy-paste mechanism for prototypes and typedefs) and
put the modules in the compiler's command line. The Che translator does
that automatically:

	// main.c:
	----------------
	import "module2"

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

The translator will replace every import statement with type
declarations and function prototypes extracted from the referenced
module. This is what C programmers do, except they put the declarations
into a separate file and include it. The translator will also track all
the referenced modules and put them to the C compiler's command line.

Modules are searched first in the internal modules library, and then in
current working directory. If import name begins with "./", the module
is searched relatively to the directory of the importing module.

A module can be split into several files to better organize the code.
In this case the containing folder must be named in the import
statement. The separate files are then not treated as modules, but as
incomplete module files. When importing, these files will be merged to
obtain the module which will finally be imported and compiled. So, the
following package “pak”:

	pak/a.c:
		typedef int foo_t;

		pub int foo(foo_t x) {
			return bar(2*x);
		}

	pak/b.c:
		int bar(foo_t x) {
			return x*x;
		}

would be compiled as if it was a single module:

	*:
		typedef int foo_t;
		pub int foo(foo_t x) {...}
		int bar(foo_t x) {...}


## The `pub` keyword

In C, in order to make functions private to the module, they have to be
declared as `static`. But more often than not it is more convenient to
assume they all are private and explicitly mark only the “public” ones.
Thus `static` is gone, and `pub` is introduced:

	```c
	// f1 will get imported, but f2 will not.
	pub int f1() {...}
	int f2() {...}
	```

There is an exception for `main`: it's always assumed public, so “hello
world” still is:

	```c
	int main() {
		puts("Howdy, Globe!");
	}
	```

In C variables can be `static` too. There are two kinds of those. One is
function-local, the other is module-local:

```c
// *** This is C, mind you ***
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

Function-local variables are usually caches of some kind, and they are
the same as the module-local ones, only can't be seen by other
functions. We always can move `n` out, possibly renaming it to
something like `current_count`, and deal only with module-local
variables. And those, just like functions, can be assumed private to
the module, so the result will be:

	```c
	// Note that current_count will still be initialized to zero
	// just like in C.
	long seed = 42;
	int current_count;

	int count() {
		current_count++;
		return current_count;
	}

	pub long foo() {
		return seed++ * count();
	}
	```

There is no `pub` for variables.

Enums and structs may be marked `pub` to become importable. By default
all `enum` and `struct` declarations are private.
