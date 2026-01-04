const double PI = 3.141592653589793238462643383279502884197169399375105820974944;

// Puts the standard basis shape (u, v) into res.
// u and v are indexes [0..63].
// res is a 8x8 array.
pub void getshape(int u, v, double *res) {
	int pos = 0;
    for (int x = 0; x < 8; x++) {
		double xd = (double) x;
        for (int y = 0; y < 8; y++) {
			double yd = (double) y;
            res[pos++] = basisfunc(u, xd) * basisfunc(v, yd) / 4;
        }
    }
}

double basisfunc(int u, double x) {
    double val = cos((2.0*x + 1.0) * u * PI / 16.0);
    if (u == 0) val /= sqrt(2);
    return val;
}
