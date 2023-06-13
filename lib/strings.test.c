#import strings
#import test

int main() {
    char *sample = strings.newstr("%s", "   123 \r\n ");

    // trim
    sample = strings.trim(sample);
    test.streq(sample, "123");

    // casecmp
    test.truth("GET == get", strings.casecmp("GET", "get"));
    test.truth("GET != GET1", !strings.casecmp("GET", "GET1"));

    // startswith
    test.truth("abcdefg starts with abc", strings.starts_with("abcdefg", "abc"));
    test.truth("abc doesn't start with abcdefg", !strings.starts_with("abc", "abcdefg"));
    test.truth("abcdefg doesn't start with bcd", !strings.starts_with("abcdefg", "bcd"));

    return test.fails();
}
