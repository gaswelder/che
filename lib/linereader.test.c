#import linereader
#import test

int main() {
    char output[100] = {0};
    linereader.t *r = linereader.make(output, sizeof(output));

    const char *s = "hello\r\nhey";

    const char *p = s;
    p += linereader.consume(r, p);
    test.truth("no error", !linereader.err(r));
    test.truth("done", linereader.done(r));
    test.streq(output, "hello\r\n");

    linereader.reset(r);
    p += linereader.consume(r, p);
    test.truth("no error", !linereader.err(r));
    test.truth("!done", !linereader.done(r));
    test.streq(output, "hey");
    return test.fails();
}
