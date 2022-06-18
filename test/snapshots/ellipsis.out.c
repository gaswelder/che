

/* -------test/snapshots/ellipsis.c------- */


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
static int magic (char *fmt, ...);


/* -------------- */
static int magic (char *fmt, ...) {
	va_list l = {0};
	va_start(l, fmt);
	vfprintf(stderr, fmt, l);
	va_end(l);
}
