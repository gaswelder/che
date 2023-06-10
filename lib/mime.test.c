#import mime

int main() {
    int fails = 0;
    if (!test("123", NULL)) fails++;
    if (!test("bz", "application/x-bzip")) fails++;
    if (!test("htm", "text/html")) fails++;
    return fails;
}

bool test(const char *ext, const char *exp) {
    const char *got = mime.lookup(ext);
    if (!got && !exp) {
        return true;
    }
    if (!got || !exp) {
        printf("FAIL: %s != %s\n", got, exp);
        return false;
    }
    if (strcmp(got, exp)) {
        printf("FAIL: %s != %s\n", got, exp);
        return false;
    }
    return true;
}
