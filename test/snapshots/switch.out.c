

/* -------test/snapshots/switch.in.c------- */


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
int main () {
	int t = 2;
	switch (t) {
		case 2: {
		t = 1;;
		break;;
		}
		case 3: {
		t = 2;;
		break;;
		}
		default: {
		printf("Unknown zio type: %s\n", type);;
		return 1;;
		}
	
	}
	return 0;
}
