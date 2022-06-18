

/* -------test/snapshots/union.c------- */


/* -------------- */
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


/* -------------- */
struct __foo_struct;


/* -------------- */
typedef struct __foo_struct foo;


/* -------------- */
struct __foo_struct {
	size_t len;
	union {
		int i;
		void *v;
	
	} *vals;

};
