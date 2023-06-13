#import strings
#import test

int main() {
    char *sample = strings.newstr("%s", "   123 \r\n ");
    sample = strings.trim(sample);
    test.streq(sample, "123");
    return test.fails();
}
