#import strings
#import test

int main() {
    char *sample = strings.newstr("%s", "   123 \r\n ");
    sample = strings.trim(sample);
    test.streq(sample, "123");
    test.truth("GET == get", strings.casecmp("GET", "get"));
    test.truth("GET != GET1", !strings.casecmp("GET", "GET1"));
    return test.fails();
}
