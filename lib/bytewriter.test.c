#import test
#import bytewriter.c

int main() {
    uint8_t buf[3];

    bytewriter.t *w = bytewriter.tobuf(buf, sizeof(buf));
    test.truth("a", bytewriter.byte(w, 'a'));
    test.truth("b", bytewriter.byte(w, 'b'));
    test.truth("c", bytewriter.byte(w, 'c'));
    test.truth("end of buffer", !bytewriter.byte(w, 'd'));
    test.truth("memcmp", memcmp(buf, "abc", sizeof(buf)) == 0);
    bytewriter.free(w);
    return test.fails();
}
