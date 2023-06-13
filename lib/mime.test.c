#import mime
#import test

int main() {
    test.streq(mime.lookup("123"), NULL);
    test.streq(mime.lookup("bz"), "application/x-bzip");
    test.streq(mime.lookup("htm"), "text/html");
    return test.fails();
}
