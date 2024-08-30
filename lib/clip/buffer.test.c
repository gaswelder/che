#import test
#import buffer.c

int main() {
    buffer.t *buf = buffer.new();


    buffer.write(buf, "a", 1);
    buffer.write(buf, "bc", 2);
    buffer.write(buf, "def", 3);

    char tmp[10] = {};
    size_t n = buffer.read(buf, tmp, sizeof(tmp));
    test.truth("buffer.read size", n == 6);

    // dry read, keeps tmp intact.
    n = buffer.read(buf, tmp, sizeof(tmp));

    test.streq(tmp, "abcdef");

    buffer.free(buf);
    return test.fails();
}