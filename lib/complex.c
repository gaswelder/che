// Complex numbers.

pub typedef {
	double re, im;
} t;

/*
 * Returns the sum of a and b.
*/
pub t sum(t a, b) {
	a.re += b.re;
	a.im += b.im;
	return a;
}

/*
 * Returns the product of a and b.
 */
pub t mul(t a, b) {
	t r = {
		.re = a.re * b.re - a.im * b.im,
		.im = a.re * b.im + b.re * a.im
	};
	return r;	
}

pub double abs(t z) {
	return sqrt(z.re * z.re + z.im * z.im);
}

pub double abs2(t z) {
	return z.re * z.re + z.im * z.im;
}

pub void print(FILE *f, t x) {
    fprintf(f, "(%f + %fi)", x.re, x.im);
}
