

/* -------test/snapshots/literals.in.c------- */


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
static char *a[] = {
	"a",
	"b",
	"c"
};
static char *b[] = {
	"a",
	"b",
	"c"
};
static int a[] = {
	1,
	2,
	0x04 | 0x40
};
