#define _XOPEN_SOURCE 700
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <setjmp.h>
#define nelem(x) (sizeof (x)/sizeof (x)[0])
#define _XOPEN_SOURCE 700
#define nelem(x) (sizeof (x)/sizeof (x)[0])
typedef struct __ns_test_scoped_lib_c__foo_t_struct ns_test_scoped_lib_c__foo_t;
enum {
	ns_test_scoped_lib_c__ONE = 1,
	ns_test_scoped_lib_c__TWO = 2
};
struct __ns_test_scoped_lib_c__foo_t_struct {
	int a;

};
void ns_test_scoped_lib_c__f ();
static void f (const int y, ns_test_scoped_lib_c__foo_t x);
int main () {
	ns_test_scoped_lib_c__foo_t val = {0};
	printf("size = %zu\n", sizeof(val));
	printf("one = %d\b", ns_test_scoped_lib_c__ONE);
	ns_test_scoped_lib_c__f();
	switch (val.a) {
		case ns_test_scoped_lib_c__ONE: {
		break;;
		}
		default: {
		puts("not one");;
		}
	
	}
	f(10, val);
	return 0;
}
static void f (const int y, ns_test_scoped_lib_c__foo_t x) {
	printf("y=%d, a = %d\n", y, x.a);
}
