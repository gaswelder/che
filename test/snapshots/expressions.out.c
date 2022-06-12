

/* -------test/snapshots/expressions.c------- */


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


/* -------------- */
void main () {
	char optstr[MAX_FLAGS * 2 + 2] = {0};
	if (!*argv || !*argv + 1) {
		return;;
	}
	
	char foo[] = "abcdef";
	putchar('\n');
	!*foo()[3]++;
	12 * sizeof(int);
	242 * !(uint32_t)(foo);
	1 + 2 * 3 + 4;
	a + b + c;
	a + b * c;
	(a + b) * c;
	a + b * !c;
	a + --c;
	pos.usec = next->pos_usec;
	pos.min = -1 + -1;
	if (!arr_pushi(a, i)) {
		fatal("failed");
	}
	
	putch(ch, &c->out);
	sizeof(b->data);
	if ((i + 1) * (i + 1) != *(int*)(r)) {
		puts("");
	}

}
