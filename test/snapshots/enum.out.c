

/* -------test/snapshots/enum.c------- */


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
enum {
	T_ERR = 257,
	T_TRUE,
	T_FALSE,
	T_NULL,
	T_STR,
	T_NUM,
	T1 = (1 << T_ERR) - 1 ^ 0xFF
};
