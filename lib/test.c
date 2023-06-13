int _fails = 0;

pub bool streq(const char *a, *b) {
    if (!a && !b) {
        return true;
    }
    if ((!a || !b) || strcmp(a, b)) {
        printf("FAIL: %s != %s\n", a, b);
        _fails++;
        return false;
    }
    return true;
}

pub int fails() {
    return _fails;
}
