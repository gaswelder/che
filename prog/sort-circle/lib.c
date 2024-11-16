pub float clampf(float x, lower, upper) {
    if (x < lower) return lower;
    if (x > upper) return upper;
    return x;
}
