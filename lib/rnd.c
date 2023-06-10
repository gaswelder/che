const int n = 10;

pub double gauss() {
    // Brute-force approach until something better comes.
    double v = (double)rand() / RAND_MAX;
    for (int j = 0; j < n; j++) {
        v += (double)rand() / RAND_MAX;
    }
    return v / (n+1) - 0.5;
}

pub void noop() {
    //
}