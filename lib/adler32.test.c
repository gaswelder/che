#import adler32
#import test

int main() {
    uint8_t s[] = {'W','i','k','i','p','e','d','i','a'};

    uint32_t r = adler32.init();
    uint8_t *p = s;
    while (*p) {
        r = adler32.adler32(r, p, 1);
        p++;
    }
    test.truth("one byte at a time", r == 0x11E60398);


    uint32_t r2 = adler32.init();
    r2 = adler32.adler32(r2, s, nelem(s));
    test.truth("one buffer at a time", r2 == 0x11E60398);

    return test.fails();
}
