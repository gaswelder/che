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
struct __ns_test_scoped_lib_c__foo_t_struct;
typedef struct __ns_test_scoped_lib_c__foo_t_struct ns_test_scoped_lib_c__foo_t;
enum {
	ns_test_scoped_lib_c__ONE = 1,
	ns_test_scoped_lib_c__TWO = 2
};
struct __ns_test_scoped_lib_c__foo_t_struct {
	int a;

};
void ns_test_scoped_lib_c__f ();
void ns_test_scoped_lib_c__f () {
	ns_test_scoped_lib_c__foo_t x = {0};
	printf("x.a = %d\n", x.a);
	puts("OK");
}
